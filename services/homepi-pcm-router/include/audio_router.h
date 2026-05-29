#pragma once

#include <stdbool.h>

#include "config.h"
#include "dac_resolver.h"
#include "zone_state.h"

/**
 * Starts PCM capture threads and DAC writer.
 * @param cfg Configuration.
 * @param assignment Resolved DAC assignment.
 * @param zone_state Zone state.
 * @return True on success.
 */
bool audio_router_start(const HomepiConfig* cfg, const DacAssignment* assignment, ZoneState* zone_state);

/** Stops audio router threads and closes ALSA devices. */
void audio_router_stop(void);

/**
 * Notifies router that owner zone changed (for fade).
 * @param new_owner New owner zone (0 = none).
 * @param previous_owner Previous owner.
 */
void audio_router_on_owner_changed(int new_owner, int previous_owner);

/**
 * Notifies router that DAC should enter idle keepalive mode.
 */
void audio_router_on_owner_cleared(void);
