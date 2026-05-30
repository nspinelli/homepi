#include <alsa/asoundlib.h>
#include <signal.h>
#include <stdbool.h>
#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <unistd.h>

#include "audio_loopback.h"
#include "audio_router.h"
#include "config.h"
#include "dac_resolver.h"
#include "json_events.h"
#include "log.h"
#include "mqtt_client.h"
#include "nqptp_refresh.h"
#include "unix_socket_server.h"
#include "zone_state.h"

static volatile sig_atomic_t g_stop = 0;
static HomepiConfig g_cfg;
static ZoneState g_zone_state;
static char g_snapshot_payload[2048];
static bool g_audio_router_active = false;
static const char* g_dac_state_label = "DAC_DEGRADED";

static void on_signal(int signo) {
  (void)signo;
  g_stop = 1;
}

static void sync_audio_routing(void) {
  if (!g_audio_router_active) {
    return;
  }
  int stack[HOMEPI_PCM_MAX_ZONES];
  const size_t count = zone_state_copy_stack(&g_zone_state, stack, HOMEPI_PCM_MAX_ZONES);
  const int owner = zone_state_get_owner(&g_zone_state);
  audio_router_apply_routing(owner, stack, count);
}

static void on_zone_event(const char* event_name, int owner, int previous, void* user) {
  (void)user;
  if (!g_audio_router_active) {
    return;
  }
  if (strcmp(event_name, "owner_changed") == 0 && owner > 0 && previous > 0 && owner != previous) {
    char client_ip[64];
    zone_state_get_client_ip(&g_zone_state, owner, client_ip, sizeof(client_ip));
    nqptp_schedule_owner_handoff_refresh(owner, client_ip, 750);
  }
  if (strcmp(event_name, "owner_cleared") == 0) {
    mqtt_client_rebuild_routing_stack(&g_zone_state);
  }
  sync_audio_routing();
}

static void on_socket_command(const char* method, int zone_id, void* user) {
  ZoneState* state = (ZoneState*)user;
  if (!state || zone_id < 1) {
    return;
  }
  if (strcmp(method, "route_start") == 0) {
    zone_state_on_active_start(state, zone_id);
    return;
  }
  if (strcmp(method, "route_end") == 0) {
    zone_state_on_active_end(state, zone_id);
    if (zone_state_get_owner(state) == 0) {
      mqtt_client_rebuild_routing_stack(state);
    }
    return;
  }
  if (strcmp(method, "route_join") == 0) {
    zone_state_on_route_join(state, zone_id);
  }
}

static const char* snapshot_json(void) {
  int stack[HOMEPI_PCM_MAX_ZONES];
  const int owner = zone_state_get_owner(&g_zone_state);
  const size_t count = zone_state_copy_stack(&g_zone_state, stack, HOMEPI_PCM_MAX_ZONES);
  const char* dac_state = g_audio_router_active
                              ? (owner > 0 ? "DAC_OPEN" : "DAC_IDLE")
                              : g_dac_state_label;
  json_events_build_snapshot(owner, stack, count, dac_state, g_snapshot_payload, sizeof(g_snapshot_payload));
  return g_snapshot_payload;
}

static int validate_loopback_only(void) {
  if (!audio_loopback_validate_all(&g_cfg)) {
    fprintf(stderr, "ALSA loopback validation failed\n");
    return 1;
  }
  printf("ALSA loopback validation OK\n");
  return 0;
}

static int validate_dac_only(void) {
  DacAssignment assignment;
  if (!dac_resolver_load_primary(&g_cfg, &assignment)) {
    fprintf(stderr, "Primary DAC resolution failed\n");
    return 1;
  }

  snd_pcm_t* handle = NULL;
  if (snd_pcm_open(&handle, assignment.dac_device, SND_PCM_STREAM_PLAYBACK, 0) < 0) {
    fprintf(stderr, "Cannot open DAC %s\n", assignment.dac_device);
    return 1;
  }
  snd_pcm_close(handle);
  printf("DAC validation OK: %s\n", assignment.dac_device);
  return 0;
}

static int run_daemon(void) {
  DacAssignment assignment;
  const bool dac_assigned = dac_resolver_load_primary(&g_cfg, &assignment);

  if (!audio_loopback_validate_all(&g_cfg)) {
    log_msg(LOG_LEVEL_ERROR, "main", "loopback_validation_failed", "startup");
    return 1;
  }

  zone_state_init(&g_zone_state, &g_cfg);
  zone_state_set_callback(&g_zone_state, on_zone_event, NULL);

  if (!unix_socket_server_start(g_cfg.event_socket_path, snapshot_json, on_socket_command,
                                &g_zone_state)) {
    log_msg(LOG_LEVEL_ERROR, "main", "socket_start_failed", g_cfg.event_socket_path);
    return 1;
  }

  if (!mqtt_client_start(&g_cfg, &g_zone_state)) {
    log_msg(LOG_LEVEL_WARN, "main", "mqtt_start_failed", "continuing without MQTT");
  }

  if (!dac_assigned) {
    g_dac_state_label = "DAC_UNASSIGNED";
    log_msg(LOG_LEVEL_WARN, "main", "dac_unassigned",
            "Set Primary Audio Output in settings; audio routing inactive");
    json_events_emit("modules.pcm", "dac_state", "startup",
                     "{\"state\":\"DAC_UNASSIGNED\",\"reason\":\"no_primary_assignment\"}");
  } else if (audio_router_start(&g_cfg, &assignment, &g_zone_state)) {
    g_audio_router_active = true;
    g_dac_state_label = "DAC_OPEN";
    log_msg(LOG_LEVEL_INFO, "main", "started", assignment.dac_device);
    sync_audio_routing();
  } else {
    g_dac_state_label = "DAC_UNAVAILABLE";
    log_msg(LOG_LEVEL_WARN, "main", "audio_router_degraded", assignment.dac_device);
    json_events_emit("modules.pcm", "dac_state", "startup",
                     "{\"state\":\"DAC_UNAVAILABLE\",\"reason\":\"dac_open_failed\","
                     "\"device\":\"plughw:HomePiPrimary,0\"}");
  }

  if (g_audio_router_active) {
    json_events_emit("modules.pcm", "health", "startup",
                     "{\"status\":\"running\",\"audioActive\":true}");
  } else {
    json_events_emit("modules.pcm", "health", "startup",
                     "{\"status\":\"degraded\",\"audioActive\":false}");
  }

  while (!g_stop) {
    sleep(1);
  }

  if (g_audio_router_active) {
    audio_router_stop();
  }
  mqtt_client_stop();
  unix_socket_server_stop();
  zone_state_destroy(&g_zone_state);
  return 0;
}

int main(int argc, char** argv) {
  if (!config_load(&g_cfg)) {
    fprintf(stderr, "Failed to load configuration\n");
    return 1;
  }
  log_set_level(g_cfg.log_level);

  if (argc > 1 && strcmp(argv[1], "--validate-alsa") == 0) {
    return validate_loopback_only();
  }
  if (argc > 1 && strcmp(argv[1], "--validate-dac") == 0) {
    return validate_dac_only();
  }
  if (argc > 1 && strcmp(argv[1], "--validate-all") == 0) {
    if (validate_loopback_only() != 0) {
      return 1;
    }
    return validate_dac_only();
  }

  signal(SIGINT, on_signal);
  signal(SIGTERM, on_signal);
  return run_daemon();
}
