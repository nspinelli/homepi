#pragma once

#include <stdbool.h>
#include <stddef.h>

/** Maximum PCM zones supported by the router. */
#define HOMEPI_PCM_MAX_ZONES 16

/** Loaded service configuration from environment. */
typedef struct HomepiConfig {
  int zone_count;
  char mqtt_host[256];
  int mqtt_port;
  char mqtt_topic_filter[256];
  char database_path[512];
  char event_socket_path[256];
  char primary_alsa_card[64];
  char loopback_card_a[64];
  char loopback_card_b[64];
  unsigned int audio_rate;
  unsigned int audio_channels;
  char audio_format[32];
  unsigned int period_frames;
  unsigned int buffer_frames;
  unsigned int fade_ms;
  unsigned int metadata_debounce_ms;
  unsigned int dac_idle_keepalive_ms;
  char log_level[32];
} HomepiConfig;

/**
 * Loads configuration from environment variables.
 * @param out Output configuration.
 * @return True on success.
 */
bool config_load(HomepiConfig* out);

/**
 * Builds ALSA hw device string for the configured primary DAC.
 * @param cfg Service configuration.
 * @param out Output buffer.
 * @param out_len Buffer length.
 */
void config_primary_dac_device(const HomepiConfig* cfg, char* out, size_t out_len);
