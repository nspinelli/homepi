#include "audio_router.h"

#include <alsa/asoundlib.h>
#include <math.h>
#include <pthread.h>
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

typedef struct CaptureContext {
  int zone_id;
  const HomepiConfig* cfg;
  ZoneState* zone_state;
  PcmRingBuffer ring;
  snd_pcm_t* handle;
  pthread_t thread;
  bool running;
} CaptureContext;

static CaptureContext g_captures[HOMEPI_PCM_MAX_ZONES];
static int g_capture_count = 0;

static snd_pcm_t* g_dac_handle = NULL;
static pthread_t g_dac_thread;
static bool g_router_running = false;
static bool g_dac_thread_running = false;
static bool g_dac_thread_started = false;

static _Atomic int g_owner_zone = 0;
static _Atomic int g_fade_target_owner = 0;
static _Atomic double g_fade_gain = 1.0;
static _Atomic bool g_dac_open = false;
static _Atomic bool g_idle_keepalive = false;

static const HomepiConfig* g_cfg = NULL;
static char g_dac_device[128];
static unsigned int g_period_frames = 256;

static snd_pcm_format_t parse_format(const char* name) {
  if (strcmp(name, "S32_LE") == 0) {
    return SND_PCM_FORMAT_S32_LE;
  }
  if (strcmp(name, "S16_LE") == 0) {
    return SND_PCM_FORMAT_S16_LE;
  }
  return SND_PCM_FORMAT_S32_LE;
}

static bool configure_pcm(snd_pcm_t* handle, const HomepiConfig* cfg) {
  snd_pcm_hw_params_t* params = NULL;
  snd_pcm_hw_params_alloca(&params);
  snd_pcm_hw_params_any(handle, params);
  snd_pcm_hw_params_set_access(handle, params, SND_PCM_ACCESS_RW_INTERLEAVED);
  snd_pcm_hw_params_set_format(handle, params, parse_format(cfg->audio_format));
  snd_pcm_hw_params_set_channels(handle, params, cfg->audio_channels);
  unsigned int rate = cfg->audio_rate;
  snd_pcm_hw_params_set_rate_near(handle, params, &rate, 0);
  snd_pcm_uframes_t period = cfg->period_frames;
  snd_pcm_hw_params_set_period_size_near(handle, params, &period, 0);
  snd_pcm_uframes_t buffer = cfg->buffer_frames;
  snd_pcm_hw_params_set_buffer_size_near(handle, params, &buffer);
  if (snd_pcm_hw_params(handle, params) < 0) {
    return false;
  }
  snd_pcm_prepare(handle);
  return true;
}

static void s32_to_float(const int32_t* in, float* out, size_t samples) {
  for (size_t i = 0; i < samples; ++i) {
    out[i] = (float)in[i] / 2147483648.0f;
  }
}

static void float_to_s32(const float* in, int32_t* out, size_t samples, float gain) {
  for (size_t i = 0; i < samples; ++i) {
    const float scaled = in[i] * gain;
    float clamped = scaled;
    if (clamped > 1.0f) {
      clamped = 1.0f;
    }
    if (clamped < -1.0f) {
      clamped = -1.0f;
    }
    out[i] = (int32_t)(clamped * 2147483647.0f);
  }
}

static void* capture_thread(void* arg) {
  CaptureContext* ctx = (CaptureContext*)arg;
  const size_t channels = g_cfg->audio_channels;
  const size_t frame_bytes = channels * sizeof(int32_t);
  int32_t* raw = (int32_t*)malloc(g_period_frames * frame_bytes);
  float* converted = (float*)malloc(g_period_frames * channels * sizeof(float));

  while (ctx->running) {
    snd_pcm_sframes_t frames =
        snd_pcm_readi(ctx->handle, raw, (snd_pcm_uframes_t)g_period_frames);
    if (frames < 0) {
      snd_pcm_recover(ctx->handle, (int)frames, 1);
      continue;
    }
    if (frames == 0) {
      continue;
    }

    s32_to_float(raw, converted, (size_t)frames * channels);
    const int owner = atomic_load(&g_owner_zone);
    if (owner == ctx->zone_id) {
      ringbuffer_write(&ctx->ring, converted, (size_t)frames);
    }
  }

  free(raw);
  free(converted);
  return NULL;
}

static void* dac_thread(void* arg) {
  (void)arg;
  const size_t channels = g_cfg->audio_channels;
  const size_t samples_per_period = g_period_frames * channels;
  float* in_buf = (float*)calloc(samples_per_period, sizeof(float));
  int32_t* out_buf = (int32_t*)calloc(samples_per_period, sizeof(int32_t));

  const unsigned int fade_frames =
      (g_cfg->fade_ms * g_cfg->audio_rate) / 1000U;
  unsigned int fade_position = 0;
  int fade_from_owner = 0;

  while (g_dac_thread_running) {
    if (!atomic_load(&g_dac_open) || !g_dac_handle) {
      usleep(10000);
      continue;
    }

    const int owner = atomic_load(&g_owner_zone);
    const int fade_target = atomic_load(&g_fade_target_owner);
    double gain = atomic_load(&g_fade_gain);

    if (fade_target != 0 && fade_position < fade_frames) {
      const double t = (double)fade_position / (double)(fade_frames == 0 ? 1 : fade_frames);
      gain = 1.0 - t;
      fade_position++;
      if (fade_position >= fade_frames) {
        atomic_store(&g_fade_gain, 1.0);
        atomic_store(&g_fade_target_owner, 0);
        fade_position = 0;
      }
    }

    size_t got = 0;
    if (owner > 0 && owner <= g_capture_count) {
      got = ringbuffer_read(&g_captures[owner - 1].ring, in_buf, g_period_frames);
    }

    if (got < g_period_frames) {
      memset(in_buf + got * channels, 0, (g_period_frames - got) * channels * sizeof(float));
    }

    float_to_s32(in_buf, out_buf, samples_per_period, (float)gain);
    snd_pcm_sframes_t written =
        snd_pcm_writei(g_dac_handle, out_buf, (snd_pcm_uframes_t)g_period_frames);
    if (written < 0) {
      snd_pcm_recover(g_dac_handle, (int)written, 1);
    }

    if (owner == 0 && atomic_load(&g_idle_keepalive)) {
      static time_t last_keepalive = 0;
      const time_t now = time(NULL);
      if (last_keepalive == 0) {
        last_keepalive = now;
      }
      if ((unsigned int)(now - last_keepalive) * 1000U > g_cfg->dac_idle_keepalive_ms) {
        snd_pcm_close(g_dac_handle);
        g_dac_handle = NULL;
        atomic_store(&g_dac_open, false);
        json_events_emit("modules.pcm", "dac_state", "dac-idle",
                         "{\"state\":\"DAC_CLOSED\",\"reason\":\"idle_keepalive_expired\"}");
        last_keepalive = 0;
      }
    }
    (void)fade_from_owner;
  }

  free(in_buf);
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
  if (!configure_pcm(g_dac_handle, g_cfg)) {
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
  g_period_frames = cfg->period_frames;
  snprintf(g_dac_device, sizeof(g_dac_device), "%s", assignment->dac_device);

  if (!open_dac()) {
    return false;
  }

  for (int i = 0; i < cfg->zone_count; ++i) {
    CaptureContext* ctx = &g_captures[i];
    ctx->zone_id = i + 1;
    ctx->cfg = cfg;
    ctx->zone_state = zone_state;
    if (!ringbuffer_init(&ctx->ring, 8, cfg->audio_channels)) {
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
    if (!configure_pcm(ctx->handle, cfg)) {
      return false;
    }
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
}

void audio_router_on_owner_changed(int new_owner, int previous_owner) {
  if (!g_router_running) {
    return;
  }
  (void)previous_owner;
  atomic_store(&g_fade_target_owner, new_owner);
  atomic_store(&g_fade_gain, 0.0);
  atomic_store(&g_owner_zone, new_owner);
  atomic_store(&g_idle_keepalive, false);
  if (!atomic_load(&g_dac_open)) {
    open_dac();
  }
}

void audio_router_on_owner_cleared(void) {
  if (!g_router_running) {
    return;
  }
  atomic_store(&g_owner_zone, 0);
  atomic_store(&g_idle_keepalive, true);
  for (int i = 0; i < g_capture_count; ++i) {
    ringbuffer_clear(&g_captures[i].ring);
  }
}
