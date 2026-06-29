#pragma once

#include <string>

namespace homepi::audio_orchestrator {

/** Runtime configuration for homepi-audio-orchestrator. */
struct ServiceConfig {
  std::string service = "homepi-audio-orchestrator";
  std::string events_socket = "/run/homepi/broker/broker.sock";
  std::string pcm_router_socket = "/run/homepi/audio/pcm-router.sock";
  std::string hifi_serial_socket = "/run/homepi/audio/hifi-serial.sock";
  std::string database_path = "/opt/homepi/runtime/state/homepi.sqlite";
  std::string nqptp_host = "127.0.0.1";
  int nqptp_port = 9000;
  int default_airplay_source = 5;
};

/**
 * Loads service configuration from a JSON file path and environment overrides.
 * @param config_path Path to service-config.json.
 * @returns Populated configuration.
 */
ServiceConfig load_service_config(const std::string& config_path);

}  // namespace homepi::audio_orchestrator
