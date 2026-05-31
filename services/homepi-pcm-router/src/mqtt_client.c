#include "mqtt_client.h"

#include <mosquitto.h>
#include <pthread.h>
#include <stdbool.h>
#include <string.h>
#include <unistd.h>

#include "log.h"
#include "shairport_metadata_adapter.h"

static struct mosquitto* g_mosq = NULL;
static pthread_t g_thread;
static bool g_stop = false;
static bool g_connected = false;
static ZoneState* g_zone_state = NULL;
static HomepiConfig g_cfg;
static bool g_zone_session_active[HOMEPI_PCM_MAX_ZONES + 1];
static bool g_zone_playing[HOMEPI_PCM_MAX_ZONES + 1];

static bool payload_is_true(const char* payload) {
  return payload != NULL && (payload[0] == '1' || strcmp(payload, "true") == 0);
}

static void rebuild_routing_stack(void) {
  if (!g_zone_state || zone_state_get_owner(g_zone_state) > 0) {
    return;
  }
  for (int zone_id = 1; zone_id <= g_cfg.zone_count; ++zone_id) {
    if (g_zone_session_active[zone_id] || g_zone_playing[zone_id]) {
      zone_state_on_active_start(g_zone_state, zone_id);
    }
  }
}

static void on_connect(struct mosquitto* mosq, void* userdata, int rc) {
  (void)userdata;
  if (rc == 0) {
    g_connected = true;
    mosquitto_subscribe(mosq, NULL, g_cfg.mqtt_topic_filter, 0);
    log_msg(LOG_LEVEL_INFO, "mqtt_client", "connected", g_cfg.mqtt_host);
  } else {
    g_connected = false;
    log_msg(LOG_LEVEL_WARN, "mqtt_client", "connect_failed", "mosquitto");
  }
}

static void on_disconnect(struct mosquitto* mosq, void* userdata, int rc) {
  (void)mosq;
  (void)userdata;
  (void)rc;
  g_connected = false;
  log_msg(LOG_LEVEL_WARN, "mqtt_client", "disconnected", "mosquitto");
}

static void on_message(struct mosquitto* mosq, void* userdata,
                       const struct mosquitto_message* message) {
  (void)mosq;
  (void)userdata;
  if (!message || !message->topic || !g_zone_state) {
    return;
  }

  ShairportTopic parsed;
  shairport_topic_parse(message->topic, &parsed);
  if (!parsed.valid) {
    return;
  }

  char payload[512];
  if (message->payload && message->payloadlen > 0) {
    const size_t len = (size_t)message->payloadlen;
    const size_t copy = len < sizeof(payload) - 1 ? len : sizeof(payload) - 1;
    memcpy(payload, message->payload, copy);
    payload[copy] = '\0';
  } else {
    payload[0] = '\0';
  }

  if (strcmp(parsed.field, "route_start") == 0) {
    g_zone_session_active[parsed.zone_id] = true;
    zone_state_on_active_start(g_zone_state, parsed.zone_id);
    return;
  }
  if (strcmp(parsed.field, "route_end") == 0) {
    g_zone_session_active[parsed.zone_id] = false;
    g_zone_playing[parsed.zone_id] = false;
    zone_state_on_active_end(g_zone_state, parsed.zone_id);
    if (zone_state_get_owner(g_zone_state) == 0) {
      rebuild_routing_stack();
    }
    return;
  }
  if (strcmp(parsed.field, "route_join") == 0) {
    zone_state_on_route_join(g_zone_state, parsed.zone_id);
    return;
  }
  if (strcmp(parsed.field, "active_start") == 0 || strcmp(parsed.field, "active_end") == 0) {
    return;
  }
  if (strcmp(parsed.field, "active") == 0) {
    if (parsed.zone_id >= 1 && parsed.zone_id <= HOMEPI_PCM_MAX_ZONES) {
      const bool active = payload_is_true(payload);
      g_zone_session_active[parsed.zone_id] = active;
      if (active) {
        if (zone_state_get_owner(g_zone_state) == 0) {
          zone_state_on_active_start(g_zone_state, parsed.zone_id);
        }
      } else {
        g_zone_playing[parsed.zone_id] = false;
        zone_state_on_active_end(g_zone_state, parsed.zone_id);
        if (zone_state_get_owner(g_zone_state) == 0) {
          rebuild_routing_stack();
        }
      }
    }
    return;
  }
  if (strcmp(parsed.field, "playing") == 0) {
    if (parsed.zone_id >= 1 && parsed.zone_id <= HOMEPI_PCM_MAX_ZONES) {
      const bool playing = payload_is_true(payload);
      g_zone_playing[parsed.zone_id] = playing;
      if (playing && zone_state_get_owner(g_zone_state) == 0) {
        zone_state_on_active_start(g_zone_state, parsed.zone_id);
      }
      /* Paused playback keeps routing; route_end/active=false tear down. */
    }
    return;
  }

  zone_state_set_metadata(g_zone_state, parsed.zone_id, parsed.field, payload);
}

static void* mqtt_loop(void* arg) {
  (void)arg;
  int rebuild_ticks = 0;
  while (!g_stop) {
    const int rc = mosquitto_loop(g_mosq, 100, 1);
    if (rc != MOSQ_ERR_SUCCESS) {
      g_connected = false;
      mosquitto_reconnect(g_mosq);
      usleep(500000);
    } else if (g_connected && rebuild_ticks < 15) {
      rebuild_ticks++;
      if (rebuild_ticks == 15 && zone_state_get_owner(g_zone_state) == 0) {
        rebuild_routing_stack();
      }
    }
  }
  return NULL;
}

bool mqtt_client_start(const HomepiConfig* cfg, ZoneState* zone_state) {
  g_zone_state = zone_state;
  g_cfg = *cfg;
  g_stop = false;
  memset(g_zone_session_active, 0, sizeof(g_zone_session_active));
  memset(g_zone_playing, 0, sizeof(g_zone_playing));

  mosquitto_lib_init();
  g_mosq = mosquitto_new("homepi-pcm-router", true, NULL);
  if (!g_mosq) {
    return false;
  }

  mosquitto_connect_callback_set(g_mosq, on_connect);
  mosquitto_disconnect_callback_set(g_mosq, on_disconnect);
  mosquitto_message_callback_set(g_mosq, on_message);

  if (mosquitto_connect(g_mosq, cfg->mqtt_host, cfg->mqtt_port, 60) != MOSQ_ERR_SUCCESS) {
    log_msg(LOG_LEVEL_WARN, "mqtt_client", "initial_connect_failed", cfg->mqtt_host);
  }

  pthread_create(&g_thread, NULL, mqtt_loop, NULL);
  return true;
}

void mqtt_client_stop(void) {
  g_stop = true;
  pthread_join(g_thread, NULL);
  if (g_mosq) {
    mosquitto_destroy(g_mosq);
    g_mosq = NULL;
  }
  mosquitto_lib_cleanup();
}

bool mqtt_client_is_connected(void) { return g_connected; }

void mqtt_client_rebuild_routing_stack(ZoneState* zone_state) {
  g_zone_state = zone_state;
  rebuild_routing_stack();
}
