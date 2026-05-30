#pragma once

#include <stddef.h>
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
 * Applies stack-aware capture modes and selects the playback owner.
 * Stack zones with audio data are buffered; other stack zones and all
 * non-stack zones are drained (read and discarded) to keep loopback healthy.
 * @param preferred_owner Hook-selected owner from the active stack top.
 * @param stack Ordered active zone ids (index 0 = highest priority).
 * @param stack_count Number of entries in stack.
 */
void audio_router_apply_routing(int preferred_owner, const int* stack, size_t stack_count);

/**
 * Notifies router that DAC should enter idle keepalive mode.
 */
void audio_router_on_owner_cleared(void);
