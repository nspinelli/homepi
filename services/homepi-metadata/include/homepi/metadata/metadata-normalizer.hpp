#pragma once

#include "homepi/metadata/now-playing-state.hpp"

namespace homepi::metadata {

/**
 * Returns true when a string looks like a Shairport mper persistent id (0x…).
 * @param value Candidate metadata value.
 * @returns Whether the value matches persistent-id format.
 */
bool is_persistent_id_like(const std::string& value);

/**
 * Applies display-oriented metadata cleanup after field updates.
 * @param snapshot Mutable now-playing snapshot.
 */
void normalize_now_playing_snapshot(NowPlayingSnapshot& snapshot);

}  // namespace homepi::metadata
