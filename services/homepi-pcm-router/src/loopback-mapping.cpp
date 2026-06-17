#include "homepi/pcm-router/loopback-mapping.hpp"

#include <cstdio>

namespace homepi::pcm_router {

std::optional<LoopbackDevices> map_zone_capture_device(int zone_id, const ServiceConfig& config) {
  if (zone_id < 1 || zone_id > config.zone_count) {
    return std::nullopt;
  }

  const char* card = zone_id <= 8 ? config.loopback_card_a.c_str() : config.loopback_card_b.c_str();
  const int substream = zone_id <= 8 ? zone_id - 1 : zone_id - 9;
  LoopbackDevices devices;
  char buffer[128];
  std::snprintf(buffer, sizeof(buffer), "hw:%s,1,%d", card, substream);
  devices.capture = buffer;
  return devices;
}

}  // namespace homepi::pcm_router
