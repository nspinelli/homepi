#pragma once

#include <stddef.h>

#include "config.h"

/**
 * Builds loopback capture device string for a zone.
 * @param zone_id Zone 1–16.
 * @param cfg Configuration.
 * @param out Output buffer.
 * @param out_len Buffer length.
 * @return True when zone_id is valid.
 */
bool audio_loopback_capture_device(int zone_id, const HomepiConfig* cfg, char* out, size_t out_len);

/**
 * Builds loopback playback device string for a zone.
 * @param zone_id Zone 1–16.
 * @param cfg Configuration.
 * @param out Output buffer.
 * @param out_len Buffer length.
 * @return True when zone_id is valid.
 */
bool audio_loopback_playback_device(int zone_id, const HomepiConfig* cfg, char* out, size_t out_len);

/**
 * Validates that loopback cards and all zone substreams are openable.
 * @param cfg Configuration.
 * @return True when validation passes.
 */
bool audio_loopback_validate_all(const HomepiConfig* cfg);
