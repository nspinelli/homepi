#pragma once

#include <optional>
#include <string>
#include <vector>

namespace homepi::shairport_sync {

/** Supervisor lifecycle states. */
enum class SupervisorState {
  offline,
  configuring,
  running,
};

/** Service runtime configuration. */
struct ServiceConfig {
  std::string service = "homepi-shairport-supervisor";
  std::string log_level = "INFO";
  std::string install_root = "/opt/homepi/services/shairport";
  std::string state_dir = "/opt/homepi/runtime/state";
  std::string socket_dir = "/run/homepi";
  std::string database_path = "/opt/homepi/runtime/state/homepi.sqlite";
  std::string hifi_socket_path = "/run/homepi/hifi-serial.sock";
  std::string pcm_router_socket_path = "/run/homepi/pcm-router.sock";
  std::string usb_devices_socket_path = "/run/homepi/usb-devices.sock";
  std::string artifact_path = "/opt/homepi/runtime/generated/audio/operating-profile.json";
  std::string supervisor_socket_path = "/run/homepi/shairport-supervisor.sock";
  std::string zones_config_dir = "/opt/homepi/services/shairport/config/zones";
  std::string hooks_dir = "/opt/homepi/services/shairport/bin/hooks";
  std::string shairport_binary = "/opt/homepi/services/shairport/bin/shairport-sync";
  std::string upstream_version = "4.3.6";
  int health_interval_sec = 30;
  int zone_count = 16;
};

/** Zone row used for config generation. */
struct ZoneRow {
  int zone_number = 0;
  std::optional<std::string> name;
  std::optional<int> enabled;
  std::optional<int> initial_volume;
};

/** Per-zone editable Shairport settings. */
struct ZoneSettings {
  int zone_number = 0;
  std::string volume_control_profile = "standard";
  double active_state_timeout = 1.0;
  int session_timeout = 60;
  int log_verbosity = 1;
};

/** Readiness gate evaluation result. */
struct ReadinessResult {
  bool ready = false;
  std::vector<std::string> failures;
};

/** Supervisor health for API consumers. */
struct SupervisorHealth {
  SupervisorState state = SupervisorState::offline;
  std::vector<std::string> failed_prerequisites;
  int active_zone_count = 0;
};

}  // namespace homepi::shairport_sync
