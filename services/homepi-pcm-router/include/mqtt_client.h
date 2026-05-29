#pragma once

#include <stdbool.h>

#include "config.h"
#include "zone_state.h"

/**
 * Starts MQTT subscriber thread.
 * @param cfg Configuration.
 * @param zone_state Zone state to update.
 * @return True on success.
 */
bool mqtt_client_start(const HomepiConfig* cfg, ZoneState* zone_state);

/** Stops MQTT client. */
void mqtt_client_stop(void);

/** Returns true when MQTT is connected. */
bool mqtt_client_is_connected(void);
