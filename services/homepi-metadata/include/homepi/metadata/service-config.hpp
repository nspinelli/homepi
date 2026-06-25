#pragma once

#include <cstdint>
#include <string>

namespace homepi::metadata {

/** Runtime configuration for homepi-metadata. */
struct ServiceConfig {
  std::string service = "homepi-metadata";
  std::string socket_path = "/run/homepi/metadata.sock";
  std::string events_socket = "/run/homepi/events.sock";
  std::string realtime_socket_path = "/run/homepi/audio-realtime.sock";
  std::string database_path = "/opt/homepi/runtime/state/homepi.sqlite";
  std::string cache_dir = "/opt/homepi/runtime/cache";
  std::string pipe_prefix = "/tmp/homepi-metadata-zone-";
  std::string pcm_router_socket = "/run/homepi/pcm-router.sock";
  std::string log_level = "INFO";
  int zone_count = 16;
  int metadata_debounce_ms = 250;
};

/**
 * Loads service configuration from environment variables.
 * @returns Populated configuration.
 */
ServiceConfig load_config_from_env();

}  // namespace homepi::metadata
