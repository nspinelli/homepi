#include "zone_state.h"

#include <stdio.h>
#include <string.h>

#include "json_events.h"

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
  pthread_mutex_unlock(&state->mutex);

  emit_owner_event("owner_changed", owner, previous);

  char payload[128];
  snprintf(payload, sizeof(payload), "{\"zoneId\":%d,\"active\":true}", zone_id);
  json_events_emit("modules.pcm", "zone_updated", "zone-active", payload);
}

bool zone_state_on_active_end(ZoneState* state, int zone_id) {
  if (zone_id < 1 || zone_id > HOMEPI_PCM_MAX_ZONES) {
    return false;
  }

  pthread_mutex_lock(&state->mutex);
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
  pthread_mutex_unlock(&state->mutex);

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

void zone_state_set_metadata(ZoneState* state, int zone_id, const char* field, const char* value) {
  if (zone_id < 1 || zone_id > HOMEPI_PCM_MAX_ZONES || !field || !value) {
    return;
  }
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
