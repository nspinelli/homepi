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

  if (strcmp(parsed.field, "active_start") == 0) {
    zone_state_on_active_start(g_zone_state, parsed.zone_id);
    return;
  }
  if (strcmp(parsed.field, "active_end") == 0) {
    zone_state_on_active_end(g_zone_state, parsed.zone_id);
    return;
  }

  zone_state_set_metadata(g_zone_state, parsed.zone_id, parsed.field, payload);
}

static void* mqtt_loop(void* arg) {
  (void)arg;
  while (!g_stop) {
    const int rc = mosquitto_loop(g_mosq, 100, 1);
    if (rc != MOSQ_ERR_SUCCESS) {
      g_connected = false;
      mosquitto_reconnect(g_mosq);
      usleep(500000);
    }
  }
  return NULL;
}

bool mqtt_client_start(const HomepiConfig* cfg, ZoneState* zone_state) {
  g_zone_state = zone_state;
  g_cfg = *cfg;
  g_stop = false;

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
