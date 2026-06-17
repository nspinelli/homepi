#include "homepi/pcm-router/snapshot-builder.hpp"

#include <sstream>

namespace homepi::pcm_router {

namespace {

const char* dac_state_to_string(DacLifecycleState state) {
  switch (state) {
    case DacLifecycleState::Unassigned:
      return "unassigned";
    case DacLifecycleState::Unavailable:
      return "unavailable";
    case DacLifecycleState::Idle:
      return "idle";
    case DacLifecycleState::Open:
      return "open";
    case DacLifecycleState::Paused:
      return "paused";
  }
  return "unassigned";
}

const char* bridge_state_to_string(AudioBridgeState state) {
  switch (state) {
    case AudioBridgeState::Stopped:
      return "stopped";
    case AudioBridgeState::Running:
      return "running";
    case AudioBridgeState::Paused:
      return "paused";
    case AudioBridgeState::Degraded:
      return "degraded";
  }
  return "stopped";
}

std::string tuple_json(const AudioProfileTuple& tuple) {
  std::ostringstream out;
  out << "{\"sampleRate\":" << tuple.sample_rate << ",\"channels\":" << tuple.channels
      << ",\"sampleFormat\":\"" << homepi::storage::sample_format_to_string(tuple.sample_format)
      << "\"}";
  return out.str();
}

}  // namespace

std::string SnapshotBuilder::build_payload(const RoutingState& routing, const AudioBridge& bridge,
                                           const ActiveAudioConfig& config) {
  const auto stack = routing.active_stack();
  const auto stats = bridge.stats();
  std::ostringstream out;
  out << "{"
      << "\"ownerZoneId\":" << routing.owner_zone_id() << ",\"activeStack\":[";
  for (size_t i = 0; i < stack.size(); ++i) {
    if (i > 0) {
      out << ',';
    }
    out << stack[i];
  }
  out << "],\"dacState\":\"" << dac_state_to_string(bridge.dac_state()) << "\","
      << "\"profileMode\":\""
      << (config.mode == ProfileMode::DacAssigned ? "dac_assigned" : "no_dac_assigned") << "\","
      << "\"profileStatus\":\""
      << (config.status == ProfileStatus::PausedInvalid ? "paused_invalid" : "active") << "\","
      << "\"loopbackProfile\":" << tuple_json(config.loopback_profile) << ","
      << "\"profileRevision\":" << config.profile_revision << ","
      << "\"profileSource\":\""
      << homepi::storage::profile_source_to_string(config.profile_source) << "\","
      << "\"audioBridgeState\":\"" << bridge_state_to_string(bridge.bridge_state()) << "\","
      << "\"stats\":{\"captureXruns\":" << stats.capture_xruns
      << ",\"playbackXruns\":" << stats.playback_xruns << ",\"framesCopied\":" << stats.frames_copied
      << "}";
  if (config.dac_profile.has_value()) {
    out << ",\"dacProfile\":" << tuple_json(*config.dac_profile);
  }
  if (!config.alsa_dac_device.empty()) {
    out << ",\"alsaDacDevice\":\"" << config.alsa_dac_device << "\"";
  }
  out << '}';
  return out.str();
}

}  // namespace homepi::pcm_router
