#include "json_events.h"

#include <stdio.h>
#include <string.h>
#include <time.h>

#include "unix_socket_server.h"

static JsonEventSink g_sink = NULL;
static void* g_sink_user = NULL;
static unsigned long g_event_counter = 0;

void json_events_set_sink(JsonEventSink sink, void* user) {
  g_sink = sink;
  g_sink_user = user;
}

static void iso_timestamp(char* out, size_t out_len) {
  struct timespec ts;
  clock_gettime(CLOCK_REALTIME, &ts);
  struct tm tm;
  gmtime_r(&ts.tv_sec, &tm);
  strftime(out, out_len, "%Y-%m-%dT%H:%M:%S.000Z", &tm);
}

void json_events_emit_service_status(const char* event, const char* correlation_id,
                                     const char* status, const char* extra_json) {
  char payload[512];
  if (extra_json && extra_json[0] != '\0') {
    snprintf(payload, sizeof(payload), "{\"status\":\"%s\"%s}", status,
             extra_json[0] == ',' ? extra_json : "");
  } else {
    snprintf(payload, sizeof(payload), "{\"status\":\"%s\"}", status);
  }
  json_events_emit("system.service", event, correlation_id, payload);
}

void json_events_emit(const char* topic, const char* event, const char* correlation_id,
                      const char* payload_json) {
  char timestamp[40];
  iso_timestamp(timestamp, sizeof(timestamp));

  char line[4096];
  const unsigned long id = ++g_event_counter;
  snprintf(line, sizeof(line),
           "{\"version\":1,\"id\":\"evt-pcm-%lu\",\"source\":\"homepi-pcm-router\","
           "\"topic\":\"%s\",\"event\":\"%s\",\"correlationId\":\"%s\",\"timestamp\":\"%s\","
           "\"payload\":%s}",
           id, topic, event, correlation_id ? correlation_id : "pcm", timestamp,
           payload_json && payload_json[0] == '{' ? payload_json : "{}");

  unix_socket_server_broadcast(line);
  if (g_sink) {
    g_sink(line, g_sink_user);
  }
}

void json_events_build_snapshot(int owner_zone_id, const int* stack, size_t stack_len,
                                const char* dac_state, char* out, size_t out_len) {
  char stack_json[256] = "[";
  for (size_t i = 0; i < stack_len; ++i) {
    char part[32];
    snprintf(part, sizeof(part), "%s%d", i == 0 ? "" : ",", stack[i]);
    strncat(stack_json, part, sizeof(stack_json) - strlen(stack_json) - 1);
  }
  strncat(stack_json, "]", sizeof(stack_json) - strlen(stack_json) - 1);
  snprintf(out, out_len,
           "{\"ownerZoneId\":%d,\"activeStack\":%s,\"dacState\":\"%s\",\"sourceType\":\"shairport\"}",
           owner_zone_id, stack_json, dac_state ? dac_state : "unknown");
}
