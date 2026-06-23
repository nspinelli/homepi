#pragma once

#include <array>
#include <string>

#include "homepi/pcm-router/types.hpp"

namespace homepi::pcm_router {

/**
 * Loads the enabled-zone mask from hifi_zones in the HomePi database.
 * @param database_path SQLite database path.
 * @param zone_count Maximum zone count to include.
 * @returns Per-zone enabled flags (index 0 unused).
 */
std::array<bool, kMaxZones + 1> load_enabled_zone_mask(const std::string& database_path,
                                                       int zone_count);

}  // namespace homepi::pcm_router
