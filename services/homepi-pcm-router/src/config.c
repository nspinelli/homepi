#include "config.h"

#include <stdio.h>
#include <stdlib.h>
#include <string.h>

static void copy_env_str(char* dest, size_t dest_len, const char* name, const char* fallback) {
  const char* value = getenv(name);
  if (!value || value[0] == '\0') {
    if (fallback) {
      snprintf(dest, dest_len, "%s", fallback);
    } else {
      dest[0] = '\0';
    }
    return;
  }
  snprintf(dest, dest_len, "%s", value);
}

static unsigned int env_uint(const char* name, unsigned int fallback) {
  const char* value = getenv(name);
  if (!value || value[0] == '\0') {
    return fallback;
  }
  return (unsigned int)strtoul(value, NULL, 10);
}

bool config_load(HomepiConfig* out) {
  if (!out) {
    return false;
  }
  memset(out, 0, sizeof(*out));

  out->zone_count = (int)env_uint("HOMEPI_ZONE_COUNT", 16);
  if (out->zone_count < 1 || out->zone_count > HOMEPI_PCM_MAX_ZONES) {
    out->zone_count = HOMEPI_PCM_MAX_ZONES;
  }

  copy_env_str(out->mqtt_host, sizeof(out->mqtt_host), "MQTT_HOST", "127.0.0.1");
  out->mqtt_port = (int)env_uint("MQTT_PORT", 1883);
  copy_env_str(out->mqtt_topic_filter, sizeof(out->mqtt_topic_filter), "MQTT_TOPIC_FILTER",
               "shairport/zone/+/+");
  copy_env_str(out->database_path, sizeof(out->database_path), "HOMEPI_DATABASE_PATH",
               "/opt/homepi/runtime/state/homepi.sqlite");
  copy_env_str(out->event_socket_path, sizeof(out->event_socket_path), "HOMEPI_EVENT_SOCKET",
               "/run/homepi/pcm-router.sock");
  copy_env_str(out->primary_alsa_card, sizeof(out->primary_alsa_card), "HOMEPI_PRIMARY_ALSA_CARD",
               "HomePiPrimary");
  copy_env_str(out->loopback_card_a, sizeof(out->loopback_card_a), "ALSA_LOOPBACK_CARDS",
               "HomePiZonesA");
  {
    const char* cards = getenv("ALSA_LOOPBACK_CARDS");
    if (cards) {
      char buf[128];
      snprintf(buf, sizeof(buf), "%s", cards);
      char* comma = strchr(buf, ',');
      if (comma) {
        *comma = '\0';
        snprintf(out->loopback_card_a, sizeof(out->loopback_card_a), "%s", buf);
        snprintf(out->loopback_card_b, sizeof(out->loopback_card_b), "%s", comma + 1);
      } else {
        snprintf(out->loopback_card_a, sizeof(out->loopback_card_a), "%s", buf);
        snprintf(out->loopback_card_b, sizeof(out->loopback_card_b), "HomePiZonesB");
      }
    } else {
      snprintf(out->loopback_card_b, sizeof(out->loopback_card_b), "HomePiZonesB");
    }
  }

  out->audio_rate = env_uint("HOMEPI_AUDIO_RATE", 44100);
  out->audio_channels = env_uint("HOMEPI_AUDIO_CHANNELS", 2);
  copy_env_str(out->audio_format, sizeof(out->audio_format), "HOMEPI_AUDIO_FORMAT", "S32_LE");
  out->dac_rate = env_uint("HOMEPI_DAC_RATE", 48000);
  copy_env_str(out->dac_format, sizeof(out->dac_format), "HOMEPI_DAC_FORMAT", "S16_LE");
  out->period_frames = env_uint("PERIOD_FRAMES", 256);
  out->buffer_frames = env_uint("BUFFER_FRAMES", 1024);
  out->fade_ms = env_uint("FADE_MS", 10);
  out->metadata_debounce_ms = env_uint("METADATA_DEBOUNCE_MS", 250);
  out->dac_idle_keepalive_ms = env_uint("DAC_IDLE_KEEPALIVE_MS", 300000);
  copy_env_str(out->log_level, sizeof(out->log_level), "LOG_LEVEL", "info");

  return true;
}

void config_primary_dac_device(const HomepiConfig* cfg, char* out, size_t out_len) {
  snprintf(out, out_len, "hw:%s,0", cfg->primary_alsa_card);
}
