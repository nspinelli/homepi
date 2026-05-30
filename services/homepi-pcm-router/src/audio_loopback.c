#include "audio_loopback.h"

#include <alsa/asoundlib.h>
#include <stdio.h>
#include <string.h>

#include "log.h"

static bool zone_mapping(int zone_id, const HomepiConfig* cfg, const char** card, int* substream) {
  if (zone_id < 1 || zone_id > cfg->zone_count) {
    return false;
  }
  if (zone_id <= 8) {
    *card = cfg->loopback_card_a;
    *substream = zone_id - 1;
    return true;
  }
  *card = cfg->loopback_card_b;
  *substream = zone_id - 9;
  return true;
}

bool audio_loopback_capture_device(int zone_id, const HomepiConfig* cfg, char* out, size_t out_len) {
  const char* card = NULL;
  int substream = 0;
  if (!zone_mapping(zone_id, cfg, &card, &substream)) {
    return false;
  }
  snprintf(out, out_len, "hw:%s,1,%d", card, substream);
  return true;
}

bool audio_loopback_playback_device(int zone_id, const HomepiConfig* cfg, char* out, size_t out_len) {
  const char* card = NULL;
  int substream = 0;
  if (!zone_mapping(zone_id, cfg, &card, &substream)) {
    return false;
  }
  snprintf(out, out_len, "hw:%s,0,%d", card, substream);
  return true;
}

static snd_pcm_format_t parse_format(const char* name) {
  if (strcmp(name, "S32_LE") == 0) {
    return SND_PCM_FORMAT_S32_LE;
  }
  if (strcmp(name, "S16_LE") == 0) {
    return SND_PCM_FORMAT_S16_LE;
  }
  return SND_PCM_FORMAT_S32_LE;
}

static bool try_open_device(const char* device, const HomepiConfig* cfg, snd_pcm_stream_t stream) {
  snd_pcm_t* handle = NULL;
  int err = snd_pcm_open(&handle, device, stream, 0);
  if (err < 0) {
    log_msg(LOG_LEVEL_ERROR, "audio_loopback", "open_failed", device);
    return false;
  }

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

  err = snd_pcm_hw_params(handle, params);
  snd_pcm_close(handle);
  if (err < 0) {
    log_msg(LOG_LEVEL_ERROR, "audio_loopback", "hw_params_failed", device);
    return false;
  }
  return true;
}

bool audio_loopback_validate_all(const HomepiConfig* cfg) {
  bool ok = true;
  for (int zone = 1; zone <= cfg->zone_count; ++zone) {
    char capture[128];
    if (!audio_loopback_capture_device(zone, cfg, capture, sizeof(capture))) {
      return false;
    }
    if (!try_open_device(capture, cfg, SND_PCM_STREAM_CAPTURE)) {
      ok = false;
    }
  }
  return ok;
}
