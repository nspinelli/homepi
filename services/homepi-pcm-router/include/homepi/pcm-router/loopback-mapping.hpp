#pragma once

#include <optional>
#include <string>

#include "homepi/pcm-router/types.hpp"

namespace homepi::pcm_router {

/** ALSA loopback device names for one zone. */
struct LoopbackDevices {
  std::string capture;
};

/**
 * Maps a zone id to loopback capture device string.
 * @param zone_id Zone id 1..16.
 * @param config Service config.
 * @return Capture device or nullopt.
 */
std::optional<LoopbackDevices> map_zone_capture_device(int zone_id, const ServiceConfig& config);

}  // namespace homepi::pcm_router
