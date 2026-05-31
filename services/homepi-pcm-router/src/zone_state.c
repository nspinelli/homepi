#include "zone_state.h"

#include <stdio.h>
#include <stdlib.h>
#include <string.h>

#include "json_events.h"
#include "log.h"

/**
 * Converts Shairport volume (Apple dB -30..0 or 0..100 percent) to 0..100.
 * @param value Raw MQTT or metadata volume string.
 * @return Percent 0..100, or -1 when unparseable.
 */
static int airplay_volume_to_percent(const char* value) {
  if (!value || value[0] == '\0') {
    return -1;
  }

  char* end = NULL;
  const double parsed = strtod(value, &end);
  if (end == value) {
    return -1;
  }

  if (value[0] == '-' || strchr(value, '.') != NULL) {
    double pct = ((parsed + 30.0) / 30.0) * 100.0;
    if (pct < 0.0) {
      pct = 0.0;
    }
    if (pct > 100.0) {
      pct = 100.0;
    }
    return (int)(pct + 0.5);
  }

  if (parsed < 0.0) {
    return 0;
  }
  if (parsed > 100.0) {
    return 100;
  }
  return (int)(parsed + 0.5);
}

struct ZoneStateCallback {
  ZoneStateEventFn fn;
  void* user;
};

static ZoneStateEventFn g_event_fn = NULL;
static void* g_event_user = NULL;

static void remove_from_stack(ZoneState* state, int zone_id) {
  size_t write = 0;
  for (size_t i = 0; i < state->active_count; ++i) {
    if (state->active_stack[i] != zone_id) {
      state->active_stack[write++] = state->active_stack[i];
    }
  }
  state->active_count = write;
}

static void push_stack(ZoneState* state, int zone_id) {
  remove_from_stack(state, zone_id);
  if (state->active_count >= HOMEPI_PCM_MAX_ZONES) {
    return;
  }
  for (size_t i = state->active_count; i > 0; --i) {
    state->active_stack[i] = state->active_stack[i - 1];
  }
  state->active_stack[0] = zone_id;
  state->active_count++;
}

static void log_stack_transition(const char* action, int zone_id, const ZoneState* state) {
  char stack_buf[128];
  size_t offset = 0;
  stack_buf[0] = '\0';
  for (size_t i = 0; i < state->active_count && offset < sizeof(stack_buf) - 8; ++i) {
    const int written =
        snprintf(stack_buf + offset, sizeof(stack_buf) - offset, "%s%d",
                 i == 0 ? "" : ",", state->active_stack[i]);
    if (written <= 0) {
      break;
    }
    offset += (size_t)written;
  }
  char detail[192];
  snprintf(detail, sizeof(detail), "zone=%d owner=%d stack=[%s]", zone_id, state->owner_zone_id,
           stack_buf);
  log_msg(LOG_LEVEL_INFO, "zone_state", action, detail);
}

static void emit_owner_event(const char* event_name, int owner, int previous) {
  char payload[512];
  snprintf(payload, sizeof(payload),
           "{\"ownerZoneId\":%d,\"previousOwnerZoneId\":%d,\"sourceType\":\"shairport\"}", owner,
           previous);
  json_events_emit("modules.pcm", event_name, "zone-state", payload);
  if (g_event_fn) {
    g_event_fn(event_name, owner, previous, g_event_user);
  }
}

void zone_state_init(ZoneState* state, const HomepiConfig* cfg) {
  (void)cfg;
  memset(state, 0, sizeof(*state));
  pthread_mutex_init(&state->mutex, NULL);
  for (int i = 0; i < HOMEPI_PCM_MAX_ZONES; ++i) {
    state->zones[i].zone_id = i + 1;
  }
  state->owner_zone_id = 0;
}

void zone_state_destroy(ZoneState* state) {
  if (!state) {
    return;
  }
  pthread_mutex_destroy(&state->mutex);
}

void zone_state_set_callback(ZoneState* state, ZoneStateEventFn fn, void* user) {
  (void)state;
  g_event_fn = fn;
  g_event_user = user;
}

void zone_state_on_active_start(ZoneState* state, int zone_id) {
  if (zone_id < 1 || zone_id > HOMEPI_PCM_MAX_ZONES) {
    return;
  }
  pthread_mutex_lock(&state->mutex);
  const int previous = state->owner_zone_id;
  push_stack(state, zone_id);
  state->zones[zone_id - 1].active = true;
  state->owner_zone_id = state->active_count > 0 ? state->active_stack[0] : 0;
  const int owner = state->owner_zone_id;
  log_stack_transition("route_start", zone_id, state);
  pthread_mutex_unlock(&state->mutex);

  if (g_event_fn) {
    g_event_fn("zone_active", zone_id, previous, g_event_user);
  }
  emit_owner_event("owner_changed", owner, previous);

  char payload[128];
  snprintf(payload, sizeof(payload), "{\"zoneId\":%d,\"active\":true}", zone_id);
  json_events_emit("modules.pcm", "zone_updated", "zone-active", payload);
}

static bool zone_in_stack(const ZoneState* state, int zone_id) {
  for (size_t i = 0; i < state->active_count; ++i) {
    if (state->active_stack[i] == zone_id) {
      return true;
    }
  }
  return false;
}

bool zone_state_on_active_end(ZoneState* state, int zone_id) {
  if (zone_id < 1 || zone_id > HOMEPI_PCM_MAX_ZONES) {
    return false;
  }

  pthread_mutex_lock(&state->mutex);
  if (!zone_in_stack(state, zone_id)) {
    const bool cleared = state->owner_zone_id == 0;
    pthread_mutex_unlock(&state->mutex);
    return cleared;
  }

  const int previous = state->owner_zone_id;
  state->zones[zone_id - 1].active = false;
  remove_from_stack(state, zone_id);
  if (state->active_count > 0) {
    state->owner_zone_id = state->active_stack[0];
  } else {
    state->owner_zone_id = 0;
  }
  const int owner = state->owner_zone_id;
  const bool cleared = owner == 0;
  log_stack_transition("route_end", zone_id, state);
  pthread_mutex_unlock(&state->mutex);

  if (g_event_fn) {
    g_event_fn("zone_inactive", zone_id, previous, g_event_user);
  }
  if (cleared) {
    emit_owner_event("owner_cleared", 0, previous);
  } else {
    emit_owner_event("owner_changed", owner, previous);
  }

  char payload[128];
  snprintf(payload, sizeof(payload), "{\"zoneId\":%d,\"active\":false}", zone_id);
  json_events_emit("modules.pcm", "zone_updated", "zone-inactive", payload);
  return cleared;
}

bool zone_state_on_route_end(ZoneState* state, int zone_id) {
  return zone_state_on_active_end(state, zone_id);
}

void zone_state_on_route_join(ZoneState* state, int zone_id) {
  if (zone_id < 1 || zone_id > HOMEPI_PCM_MAX_ZONES) {
    return;
  }

  pthread_mutex_lock(&state->mutex);
  const int previous = state->owner_zone_id;
  if (!zone_in_stack(state, zone_id) && state->active_count < HOMEPI_PCM_MAX_ZONES) {
    state->active_stack[state->active_count++] = zone_id;
  }
  state->zones[zone_id - 1].active = true;
  if (state->owner_zone_id == 0 && state->active_count > 0) {
    state->owner_zone_id = state->active_stack[0];
  }
  const int owner = state->owner_zone_id;
  pthread_mutex_unlock(&state->mutex);

  if (g_event_fn) {
    g_event_fn("zone_active", zone_id, previous, g_event_user);
  }
  if (owner != previous && owner > 0) {
    emit_owner_event("owner_changed", owner, previous);
  }

  char payload[128];
  snprintf(payload, sizeof(payload), "{\"zoneId\":%d,\"active\":true,\"joined\":true}", zone_id);
  json_events_emit("modules.pcm", "zone_updated", "zone-join", payload);
}

void zone_state_set_metadata(ZoneState* state, int zone_id, const char* field, const char* value) {
  if (zone_id < 1 || zone_id > HOMEPI_PCM_MAX_ZONES || !field || !value) {
    return;
  }

  int owner = 0;
  pthread_mutex_lock(&state->mutex);
  ZoneRecord* zone = &state->zones[zone_id - 1];
  if (strcmp(field, "title") == 0) {
    snprintf(zone->title, sizeof(zone->title), "%s", value);
  } else if (strcmp(field, "artist") == 0) {
    snprintf(zone->artist, sizeof(zone->artist), "%s", value);
  } else if (strcmp(field, "album") == 0) {
    snprintf(zone->album, sizeof(zone->album), "%s", value);
  } else if (strcmp(field, "client_name") == 0) {
    snprintf(zone->client_name, sizeof(zone->client_name), "%s", value);
  } else if (strcmp(field, "client_ip") == 0) {
    snprintf(zone->client_ip, sizeof(zone->client_ip), "%s", value);
  } else if (strcmp(field, "client_model") == 0) {
    snprintf(zone->client_model, sizeof(zone->client_model), "%s", value);
  } else if (strcmp(field, "volume") == 0) {
    snprintf(zone->volume, sizeof(zone->volume), "%s", value);
  }
  owner = state->owner_zone_id;
  const bool in_stack = zone_in_stack(state, zone_id);
  const bool zone_active = state->zones[zone_id - 1].active;
  pthread_mutex_unlock(&state->mutex);

  if (strcmp(field, "volume") == 0) {
    /* Each Shairport zone publishes its own volume; do not gate on DAC owner. */
    if (in_stack || zone_active) {
      const int pct = airplay_volume_to_percent(value);
      if (pct >= 0) {
        char volume_payload[128];
        snprintf(volume_payload, sizeof(volume_payload),
                 "{\"zoneId\":%d,\"zone\":%d,\"volume\":%d}", zone_id, zone_id, pct);
        json_events_emit("modules.pcm", "zone_volume_changed", "airplay-volume", volume_payload);
      }
    }
    return;
  }

  if (owner != zone_id) {
    return;
  }

  if (strcmp(field, "title") != 0 && strcmp(field, "artist") != 0 &&
      strcmp(field, "album") != 0 && strcmp(field, "client_name") != 0) {
    return;
  }

  char payload[768];
  snprintf(payload, sizeof(payload),
           "{\"zoneId\":%d,\"field\":\"%s\",\"value\":\"%s\"}", zone_id, field, value);
  json_events_emit("modules.pcm", "pcm_metadata_updated", "metadata", payload);
}

int zone_state_get_owner(const ZoneState* state) {
  int owner = 0;
  pthread_mutex_lock((pthread_mutex_t*)&state->mutex);
  owner = state->owner_zone_id;
  pthread_mutex_unlock((pthread_mutex_t*)&state->mutex);
  return owner;
}

size_t zone_state_copy_stack(const ZoneState* state, int* out, size_t max_len) {
  pthread_mutex_lock((pthread_mutex_t*)&state->mutex);
  size_t n = state->active_count;
  if (n > max_len) {
    n = max_len;
  }
  for (size_t i = 0; i < n; ++i) {
    out[i] = state->active_stack[i];
  }
  pthread_mutex_unlock((pthread_mutex_t*)&state->mutex);
  return n;
}

bool zone_state_get_client_ip(const ZoneState* state, int zone_id, char* out, size_t out_len) {
  if (!state || !out || out_len == 0 || zone_id < 1 || zone_id > HOMEPI_PCM_MAX_ZONES) {
    return false;
  }
  out[0] = '\0';
  pthread_mutex_lock((pthread_mutex_t*)&state->mutex);
  const char* ip = state->zones[zone_id - 1].client_ip;
  if (ip[0] != '\0') {
    snprintf(out, out_len, "%s", ip);
  }
  pthread_mutex_unlock((pthread_mutex_t*)&state->mutex);
  return out[0] != '\0';
}
