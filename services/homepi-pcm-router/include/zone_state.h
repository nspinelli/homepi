#pragma once

#include <pthread.h>
#include <stdbool.h>
#include <stddef.h>

#include "config.h"

/** Per-zone runtime metadata and activity. */
typedef struct ZoneRecord {
  int zone_id;
  bool active;
  bool in_stack;
  char title[256];
  char artist[256];
  char album[256];
  char client_name[128];
  char client_ip[64];
  char client_model[128];
  char volume[32];
} ZoneRecord;

/** Thread-safe zone owner and active stack state. */
typedef struct ZoneState {
  ZoneRecord zones[HOMEPI_PCM_MAX_ZONES];
  int active_stack[HOMEPI_PCM_MAX_ZONES];
  size_t active_count;
  int owner_zone_id;
  pthread_mutex_t mutex;
} ZoneState;

/** Callback when owner or zone state changes. */
typedef void (*ZoneStateEventFn)(const char* event_name, int owner_zone_id, int previous_owner, void* user);

/**
 * Initializes zone state.
 * @param state State object.
 * @param cfg Configuration.
 */
void zone_state_init(ZoneState* state, const HomepiConfig* cfg);

/** Destroys zone state. */
void zone_state_destroy(ZoneState* state);

/**
 * Registers event callback.
 * @param state State object.
 * @param fn Callback.
 * @param user User pointer.
 */
void zone_state_set_callback(ZoneState* state, ZoneStateEventFn fn, void* user);

/**
 * Handles MQTT active_start for a zone.
 * @param state State object.
 * @param zone_id Zone 1–16.
 */
void zone_state_on_active_start(ZoneState* state, int zone_id);

/**
 * Handles MQTT active_end for a zone.
 * @param state State object.
 * @param zone_id Zone 1–16.
 * @return True when owner was cleared (stack empty).
 */
bool zone_state_on_active_end(ZoneState* state, int zone_id);

/**
 * Updates metadata field for a zone.
 * @param state State object.
 * @param zone_id Zone id.
 * @param field Field name.
 * @param value Field value.
 */
void zone_state_set_metadata(ZoneState* state, int zone_id, const char* field, const char* value);

/**
 * Gets current owner zone (0 = none).
 * @param state State object.
 * @return Owner zone id.
 */
int zone_state_get_owner(const ZoneState* state);

/**
 * Copies active stack into caller buffer.
 * @param state State object.
 * @param out Output array.
 * @param max_len Max entries.
 * @return Number of entries copied.
 */
size_t zone_state_copy_stack(const ZoneState* state, int* out, size_t max_len);
