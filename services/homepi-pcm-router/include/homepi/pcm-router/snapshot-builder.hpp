#pragma once

#include <string>

#include "homepi/pcm-router/active-audio-config.hpp"
#include "homepi/pcm-router/audio-bridge.hpp"
#include "homepi/pcm-router/routing-state.hpp"

namespace homepi::pcm_router {

/** Builds pcm_router_snapshot payloads from in-memory runtime state. */
class SnapshotBuilder {
 public:
  /**
   * Builds snapshot JSON payload object text.
   * @param routing Routing state.
   * @param bridge Audio bridge.
   * @param config Active audio config.
   * @param zone_count Configured zone count.
   * @return JSON object without outer envelope.
   */
  static std::string build_payload(const RoutingState& routing, const AudioBridge& bridge,
                                   const ActiveAudioConfig& config, int zone_count);
};

}  // namespace homepi::pcm_router
