#include "audio_router.h"

#include <alsa/asoundlib.h>
#include <math.h>
#include <pthread.h>
#include <stdint.h>
#include <time.h>
#include <stdatomic.h>
#include <stdbool.h>
#include <stdlib.h>
#include <string.h>
#include <unistd.h>

#include "audio_loopback.h"
#include "json_events.h"
#include "log.h"
#include "ringbuffer.h"
#include <soxr.h>

typedef enum {
  CAPTURE_OFF = 0,
  CAPTURE_DRAIN = 1,
  CAPTURE_BUFFER = 2,
} CaptureMode;

typedef struct CaptureContext {
  int zone_id;
  const HomepiConfig* cfg;
  ZoneState* zone_state;
  PcmRingBuffer ring;
  snd_pcm_t* handle;
  pthread_t thread;
  unsigned int period_frames;
  bool running;
} CaptureContext;

static CaptureContext g_captures[HOMEPI_PCM_MAX_ZONES];
static int g_capture_count = 0;

static snd_pcm_t* g_dac_handle = NULL;
static pthread_t g_dac_thread;
static pthread_mutex_t g_dac_mutex = PTHREAD_MUTEX_INITIALIZER;
static pthread_cond_t g_dac_cond = PTHREAD_COND_INITIALIZER;
static pthread_mutex_t g_stack_mutex = PTHREAD_MUTEX_INITIALIZER;
static soxr_t g_soxr[HOMEPI_PCM_MAX_ZONES];
static _Atomic int g_zone_mode[HOMEPI_PCM_MAX_ZONES + 1];
static bool g_router_running = false;
static bool g_dac_thread_running = false;
static bool g_dac_thread_started = false;

static int g_stack[HOMEPI_PCM_MAX_ZONES];
static size_t g_stack_count = 0;
static _Atomic int g_preferred_owner = 0;
static _Atomic int g_playback_owner = 0;
static _Atomic bool g_dac_open = false;
static _Atomic bool g_idle_keepalive = false;

static const HomepiConfig* g_cfg = NULL;
static char g_dac_device[128];
static unsigned int g_capture_period_frames = 256;
static unsigned int g_capture_rate = 44100;
static unsigned int g_dac_period_frames = 256;
static unsigned int g_dac_rate = 48000;
static snd_pcm_format_t g_dac_pcm_format = SND_PCM_FORMAT_S16_LE;

static snd_pcm_format_t parse_format(const char* name) {
  if (strcmp(name, "S32_LE") == 0) {
    return SND_PCM_FORMAT_S32_LE;
  }
  if (strcmp(name, "S16_LE") == 0) {
    return SND_PCM_FORMAT_S16_LE;
  }
  return SND_PCM_FORMAT_S32_LE;
}

static bool configure_pcm_device(snd_pcm_t* handle, unsigned int rate, snd_pcm_format_t format,
                                 unsigned int channels, unsigned int period_frames,
                                 unsigned int buffer_frames, unsigned int* actual_rate,
                                 snd_pcm_uframes_t* actual_period, bool start_stream) {
  snd_pcm_hw_params_t* params = NULL;
  snd_pcm_hw_params_alloca(&params);
  snd_pcm_hw_params_any(handle, params);
  snd_pcm_hw_params_set_access(handle, params, SND_PCM_ACCESS_RW_INTERLEAVED);
  snd_pcm_hw_params_set_format(handle, params, format);
  snd_pcm_hw_params_set_channels(handle, params, channels);
  unsigned int requested_rate = rate;
  snd_pcm_hw_params_set_rate_near(handle, params, &requested_rate, 0);
  snd_pcm_uframes_t period = period_frames;
  snd_pcm_hw_params_set_period_size_near(handle, params, &period, 0);
  snd_pcm_uframes_t buffer = buffer_frames;
  snd_pcm_hw_params_set_buffer_size_near(handle, params, &buffer);
  if (snd_pcm_hw_params(handle, params) < 0) {
    return false;
  }
  if (actual_rate) {
    snd_pcm_hw_params_get_rate(params, actual_rate, NULL);
  }
  if (actual_period) {
    snd_pcm_hw_params_get_period_size(params, actual_period, NULL);
  }
  if (snd_pcm_prepare(handle) < 0) {
    return false;
  }
  if (start_stream && snd_pcm_start(handle) < 0) {
    return false;
  }
  return true;
}

static bool configure_capture_pcm(snd_pcm_t* handle, const HomepiConfig* cfg,
                                  unsigned int* actual_period_frames) {
  snd_pcm_uframes_t actual_period = cfg->period_frames;
  if (!configure_pcm_device(handle, cfg->audio_rate, parse_format(cfg->audio_format),
                            cfg->audio_channels, cfg->period_frames, cfg->buffer_frames,
                            &g_capture_rate, &actual_period, false)) {
    return false;
  }
  if (actual_period_frames) {
    *actual_period_frames = (unsigned int)actual_period;
  }
  return true;
}

static bool configure_dac_pcm(snd_pcm_t* handle, const HomepiConfig* cfg) {
  g_dac_pcm_format = parse_format(cfg->dac_format);
  snd_pcm_uframes_t actual_period = cfg->period_frames;
  if (!configure_pcm_device(handle, cfg->dac_rate, g_dac_pcm_format, cfg->audio_channels,
                            cfg->period_frames, cfg->buffer_frames, &g_dac_rate, &actual_period,
                            false)) {
    return false;
  }
  g_dac_period_frames = (unsigned int)actual_period;
  return true;
}

static void s32_to_float(const int32_t* in, float* out, size_t samples) {
  for (size_t i = 0; i < samples; ++i) {
    out[i] = (float)in[i] / 2147483648.0f;
  }
}

static void float_to_s16(const float* in, int16_t* out, size_t samples, float gain) {
  for (size_t i = 0; i < samples; ++i) {
    const float scaled = in[i] * gain;
    float clamped = scaled;
    if (clamped > 1.0f) {
      clamped = 1.0f;
    }
    if (clamped < -1.0f) {
      clamped = -1.0f;
    }
    out[i] = (int16_t)(clamped * 32767.0f);
  }
}

static void signal_dac_thread(void) {
  pthread_mutex_lock(&g_dac_mutex);
  pthread_cond_signal(&g_dac_cond);
  pthread_mutex_unlock(&g_dac_mutex);
}

static bool wait_for_ring_frames(PcmRingBuffer* ring, size_t needed) {
  struct timespec deadline;
  clock_gettime(CLOCK_REALTIME, &deadline);
  deadline.tv_nsec += 20000000L;
  if (deadline.tv_nsec >= 1000000000L) {
    deadline.tv_sec += 1;
    deadline.tv_nsec -= 1000000000L;
  }

  pthread_mutex_lock(&g_dac_mutex);
  while (ringbuffer_available(ring) < needed && g_dac_thread_running) {
    if (pthread_cond_timedwait(&g_dac_cond, &g_dac_mutex, &deadline) != 0) {
      break;
    }
    clock_gettime(CLOCK_REALTIME, &deadline);
    deadline.tv_nsec += 20000000L;
    if (deadline.tv_nsec >= 1000000000L) {
      deadline.tv_sec += 1;
      deadline.tv_nsec -= 1000000000L;
    }
  }
  pthread_mutex_unlock(&g_dac_mutex);
  return ringbuffer_available(ring) >= needed;
}

static bool resample_capture_to_dac(int zone_id, const float* in, size_t in_frames, float* out,
                                    size_t out_frames) {
  if (zone_id < 1 || zone_id > g_capture_count || in_frames == 0 || out_frames == 0) {
    return false;
  }
  soxr_t soxr = g_soxr[zone_id - 1];
  if (!soxr) {
    return false;
  }
  size_t idone = 0;
  size_t odone = 0;
  const soxr_error_t err = soxr_process(soxr, in, in_frames, &idone, out, out_frames, &odone);
  if (err != NULL) {
    log_msg(LOG_LEVEL_WARN, "audio_router", "soxr_process_failed", err);
    return false;
  }
  if (odone < out_frames) {
    memset(out + odone * g_cfg->audio_channels, 0,
           (out_frames - odone) * g_cfg->audio_channels * sizeof(float));
  }
  return odone > 0;
}

static void destroy_all_soxr(void) {
  for (int i = 0; i < HOMEPI_PCM_MAX_ZONES; ++i) {
    if (g_soxr[i]) {
      soxr_delete(g_soxr[i]);
      g_soxr[i] = NULL;
    }
  }
}

static bool ensure_soxr_for_zone(int zone_id) {
  if (zone_id < 1 || zone_id > g_capture_count) {
    return false;
  }
  if (g_soxr[zone_id - 1]) {
    return true;
  }
  soxr_error_t error = NULL;
  const soxr_io_spec_t io_spec = soxr_io_spec(SOXR_FLOAT32_I, SOXR_FLOAT32_I);
  const soxr_quality_spec_t quality = soxr_quality_spec(SOXR_LQ, 0);
  g_soxr[zone_id - 1] = soxr_create((double)g_capture_rate, (double)g_dac_rate, g_cfg->audio_channels,
                                    &error, &io_spec, &quality, NULL);
  if (error != NULL || !g_soxr[zone_id - 1]) {
    log_msg(LOG_LEVEL_ERROR, "audio_router", "soxr_create_failed",
            error != NULL ? error : "unknown");
    return false;
  }
  return true;
}

static void reset_soxr_for_zone(int zone_id) {
  if (zone_id < 1 || zone_id > g_capture_count) {
    return;
  }
  if (g_soxr[zone_id - 1]) {
    soxr_delete(g_soxr[zone_id - 1]);
    g_soxr[zone_id - 1] = NULL;
  }
  ensure_soxr_for_zone(zone_id);
}

static void recover_zone_capture(CaptureContext* ctx) {
  if (!ctx || !ctx->handle) {
    return;
  }
  snd_pcm_drop(ctx->handle);
  snd_pcm_prepare(ctx->handle);
  snd_pcm_start(ctx->handle);
}

static void set_zone_mode(int zone_id, CaptureMode mode) {
  if (zone_id < 1 || zone_id > g_capture_count) {
    return;
  }
  const CaptureMode previous = (CaptureMode)atomic_load(&g_zone_mode[zone_id]);
  if (previous == mode) {
    return;
  }
  atomic_store(&g_zone_mode[zone_id], (int)mode);
  CaptureContext* ctx = &g_captures[zone_id - 1];
  if (mode == CAPTURE_OFF) {
    ringbuffer_clear(&ctx->ring);
    return;
  }
  if (mode == CAPTURE_DRAIN) {
    ringbuffer_clear(&ctx->ring);
  }
  if (previous == CAPTURE_OFF) {
    recover_zone_capture(ctx);
  }
}

static void refresh_playback_owner_locked(void) {
  const int preferred = atomic_load(&g_preferred_owner);
  const int previous = atomic_load(&g_playback_owner);
  if (preferred == previous) {
    return;
  }
  if (preferred > 0) {
    reset_soxr_for_zone(preferred);
  }
  atomic_store(&g_playback_owner, preferred);
  signal_dac_thread();
}

static void* capture_thread(void* arg) {
  CaptureContext* ctx = (CaptureContext*)arg;
  const size_t channels = g_cfg->audio_channels;
  const size_t frame_bytes = channels * sizeof(int32_t);
  const unsigned int period_frames = ctx->period_frames;
  int32_t* raw = (int32_t*)malloc(period_frames * frame_bytes);
  float* converted = (float*)malloc(period_frames * channels * sizeof(float));

  while (ctx->running) {
    const CaptureMode mode = (CaptureMode)atomic_load(&g_zone_mode[ctx->zone_id]);
    if (mode == CAPTURE_OFF) {
      usleep(10000);
      continue;
    }

    snd_pcm_sframes_t frames =
        snd_pcm_readi(ctx->handle, raw, (snd_pcm_uframes_t)period_frames);
    if (frames < 0) {
      snd_pcm_recover(ctx->handle, (int)frames, 1);
      continue;
    }
    if (frames == 0) {
      usleep(1000);
      continue;
    }

    const size_t sample_count = (size_t)frames * channels;
    s32_to_float(raw, converted, sample_count);

    if (mode == CAPTURE_BUFFER) {
      ringbuffer_write(&ctx->ring, converted, (size_t)frames);
      signal_dac_thread();
    }
  }

  free(raw);
  free(converted);
  return NULL;
}

static void* dac_thread(void* arg) {
  (void)arg;
  const size_t channels = g_cfg->audio_channels;
  const size_t max_capture_frames =
      (g_capture_period_frames * g_dac_rate + (g_capture_rate - 1U)) / g_capture_rate + 1U;
  const size_t capture_samples = max_capture_frames * channels;
  const size_t dac_samples = g_dac_period_frames * channels;
  float* capture_buf = (float*)calloc(capture_samples, sizeof(float));
  float* resample_buf = (float*)calloc(dac_samples, sizeof(float));
  int16_t* out_buf = (int16_t*)calloc(dac_samples, sizeof(int16_t));

  int active_playback_owner = 0;

  while (g_dac_thread_running) {
    if (!atomic_load(&g_dac_open) || !g_dac_handle) {
      usleep(10000);
      continue;
    }

    pthread_mutex_lock(&g_stack_mutex);
    refresh_playback_owner_locked();
    const int owner = atomic_load(&g_playback_owner);
    pthread_mutex_unlock(&g_stack_mutex);

    if (owner <= 0 || owner > g_capture_count) {
      usleep(5000);
      continue;
    }

    if (owner != active_playback_owner) {
      active_playback_owner = owner;
      if (owner > 0) {
        ensure_soxr_for_zone(owner);
      }
    }

    const unsigned int capture_frames_needed =
        (g_dac_period_frames * g_capture_rate + (g_dac_rate / 2U)) / g_dac_rate;
    if (capture_frames_needed == 0) {
      usleep(1000);
      continue;
    }

    PcmRingBuffer* ring = &g_captures[owner - 1].ring;
    if (!wait_for_ring_frames(ring, capture_frames_needed)) {
      continue;
    }

    const size_t got = ringbuffer_read(ring, capture_buf, capture_frames_needed);
    if (got < capture_frames_needed) {
      continue;
    }

    if (!resample_capture_to_dac(owner, capture_buf, got, resample_buf, g_dac_period_frames)) {
      continue;
    }
    float_to_s16(resample_buf, out_buf, dac_samples, 1.0f);
    snd_pcm_sframes_t written =
        snd_pcm_writei(g_dac_handle, out_buf, (snd_pcm_uframes_t)g_dac_period_frames);
    if (written < 0) {
      snd_pcm_recover(g_dac_handle, (int)written, 1);
    }
  }

  free(capture_buf);
  free(resample_buf);
  free(out_buf);
  return NULL;
}

static bool open_dac(void) {
  if (g_dac_handle) {
    return true;
  }
  if (snd_pcm_open(&g_dac_handle, g_dac_device, SND_PCM_STREAM_PLAYBACK, 0) < 0) {
    log_msg(LOG_LEVEL_ERROR, "audio_router", "dac_open_failed", g_dac_device);
    return false;
  }
  if (!configure_dac_pcm(g_dac_handle, g_cfg)) {
    snd_pcm_close(g_dac_handle);
    g_dac_handle = NULL;
    return false;
  }
  atomic_store(&g_dac_open, true);
  json_events_emit("modules.pcm", "dac_state", "dac-open", "{\"state\":\"DAC_OPEN\"}");
  return true;
}

bool audio_router_start(const HomepiConfig* cfg, const DacAssignment* assignment, ZoneState* zone_state) {
  g_cfg = cfg;
  g_capture_count = cfg->zone_count;
  g_capture_period_frames = cfg->period_frames;
  g_dac_period_frames = cfg->period_frames;
  g_capture_rate = cfg->audio_rate;
  g_dac_rate = cfg->dac_rate;
  snprintf(g_dac_device, sizeof(g_dac_device), "%s", assignment->dac_device);

  if (!open_dac()) {
    return false;
  }

  memset(g_soxr, 0, sizeof(g_soxr));
  g_stack_count = 0;
  for (int i = 0; i < HOMEPI_PCM_MAX_ZONES + 1; ++i) {
    atomic_store(&g_zone_mode[i], (int)CAPTURE_OFF);
  }

  for (int i = 0; i < cfg->zone_count; ++i) {
    CaptureContext* ctx = &g_captures[i];
    ctx->zone_id = i + 1;
    ctx->cfg = cfg;
    ctx->zone_state = zone_state;
    const size_t ring_capacity =
        ((size_t)cfg->period_frames * cfg->dac_rate) / cfg->audio_rate + cfg->period_frames * 8U;
    if (!ringbuffer_init(&ctx->ring, ring_capacity, cfg->audio_channels)) {
      return false;
    }

    char device[128];
    if (!audio_loopback_capture_device(ctx->zone_id, cfg, device, sizeof(device))) {
      return false;
    }
    if (snd_pcm_open(&ctx->handle, device, SND_PCM_STREAM_CAPTURE, 0) < 0) {
      log_msg(LOG_LEVEL_ERROR, "audio_router", "capture_open_failed", device);
      return false;
    }
    ctx->period_frames = cfg->period_frames;
    if (!configure_capture_pcm(ctx->handle, cfg, &ctx->period_frames)) {
      return false;
    }
    if (ctx->zone_id == 1) {
      g_capture_period_frames = ctx->period_frames;
    }
    set_zone_mode(ctx->zone_id, CAPTURE_DRAIN);
    ctx->running = true;
    pthread_create(&ctx->thread, NULL, capture_thread, ctx);
  }

  g_dac_thread_running = true;
  pthread_create(&g_dac_thread, NULL, dac_thread, NULL);
  g_dac_thread_started = true;
  g_router_running = true;
  return true;
}

void audio_router_stop(void) {
  if (!g_router_running) {
    return;
  }
  g_router_running = false;
  g_dac_thread_running = false;

  for (int i = 0; i < g_capture_count; ++i) {
    g_captures[i].running = false;
    if (g_captures[i].thread) {
      pthread_join(g_captures[i].thread, NULL);
    }
    if (g_captures[i].handle) {
      snd_pcm_close(g_captures[i].handle);
      g_captures[i].handle = NULL;
    }
    ringbuffer_free(&g_captures[i].ring);
  }

  if (g_dac_thread_started) {
    pthread_join(g_dac_thread, NULL);
    g_dac_thread_started = false;
  }
  if (g_dac_handle) {
    snd_pcm_close(g_dac_handle);
    g_dac_handle = NULL;
  }
  destroy_all_soxr();
}

void audio_router_apply_routing(int preferred_owner, const int* stack, size_t stack_count) {
  if (!g_router_running) {
    return;
  }

  bool in_stack[HOMEPI_PCM_MAX_ZONES + 1];
  memset(in_stack, 0, sizeof(in_stack));
  if (stack_count > HOMEPI_PCM_MAX_ZONES) {
    stack_count = HOMEPI_PCM_MAX_ZONES;
  }

  pthread_mutex_lock(&g_stack_mutex);

  g_stack_count = stack_count;
  for (size_t i = 0; i < stack_count; ++i) {
    const int zone_id = stack[i];
    if (zone_id >= 1 && zone_id <= g_capture_count) {
      g_stack[i] = zone_id;
      in_stack[zone_id] = true;
    }
  }

  atomic_store(&g_preferred_owner, preferred_owner);
  atomic_store(&g_idle_keepalive, preferred_owner <= 0 && stack_count == 0);

  for (int zone_id = 1; zone_id <= g_capture_count; ++zone_id) {
    if (in_stack[zone_id]) {
      set_zone_mode(zone_id, CAPTURE_BUFFER);
    } else {
      set_zone_mode(zone_id, CAPTURE_DRAIN);
    }
  }

  refresh_playback_owner_locked();
  pthread_mutex_unlock(&g_stack_mutex);

  if (preferred_owner > 0 && !atomic_load(&g_dac_open)) {
    open_dac();
  }
  signal_dac_thread();
}

void audio_router_on_owner_cleared(void) {
  if (!g_router_running) {
    return;
  }
  audio_router_apply_routing(0, NULL, 0);
}
