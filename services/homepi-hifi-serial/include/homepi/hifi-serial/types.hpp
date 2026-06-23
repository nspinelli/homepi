#pragma once

#include <optional>
#include <string>
#include <vector>

namespace homepi::hifi_serial {

/** Service runtime configuration. */
struct ServiceConfig {
  std::string service = "homepi-hifi-serial";
  std::string log_level = "INFO";
  std::string state_dir = "/opt/homepi/runtime/state";
  std::string generated_dir = "/opt/homepi/runtime/generated";
  std::string socket_dir = "/run/homepi";
  std::string database_path = "/opt/homepi/runtime/state/homepi.sqlite";
  std::string socket_path = "/run/homepi/hifi-serial.sock";
  std::string virtual_port = "/dev/vHifi";
  int baud_rate = 9600;
  int command_interval_ms = 50;
  std::string events_socket = "/run/homepi/events.sock";
};

/** Daemon health for API consumers. */
struct ServiceHealth {
  std::string lifecycle = "starting";
  bool connected = false;
  std::string serial_path;
  bool serial_assigned = false;
  bool sync_in_progress = false;
  bool degraded = false;
  std::string last_full_sync_at;
  int queue_depth = 0;
};

/** Controller row. */
struct ControllerState {
  std::optional<std::string> firmware_version;
  std::optional<std::string> hardware_version;
  std::optional<std::string> device_name;
  std::optional<std::string> mac_address;
  std::optional<int> dhcp_enabled;
  std::optional<std::string> ip_address;
  std::optional<std::string> subnet_mask;
  std::optional<std::string> gateway;
  std::optional<int> tcp_port;
  std::optional<int> page_active;
  std::optional<std::string> serial_device_id;
  std::optional<std::string> serial_path;
  std::optional<std::string> last_full_sync_at;
  std::string updated_at;
};

/** Zone row. */
struct ZoneState {
  int zone_number = 0;
  std::optional<std::string> name;
  std::optional<int> enabled;
  std::optional<int> treble;
  std::optional<int> bass;
  std::optional<int> balance;
  std::optional<int> loudness;
  std::optional<int> initial_volume;
  std::optional<int> page_volume;
  std::optional<int> group_number;
  std::optional<int> power;
  std::optional<int> volume;
  std::optional<int> mute;
  std::optional<int> source;
  std::string updated_at;
};

/** Source row. */
struct SourceState {
  int source_number = 0;
  std::optional<std::string> name;
  std::optional<int> enabled;
  std::optional<int> input_gain;
  std::optional<std::string> display_line;
  std::optional<int> is_airplay;
  std::string updated_at;
};

/** Group row. */
struct GroupState {
  int group_number = 0;
  std::optional<std::string> name;
  std::optional<int> type;
  std::string updated_at;
};

/** Language string row. */
struct LanguageStringState {
  int string_number = 0;
  std::optional<std::string> value;
  std::string updated_at;
};

/** Parsed protocol update from a # response line. */
struct ParsedUpdate {
  std::string event_name;
  std::string topic;
  std::string payload_json;
};

}  // namespace homepi::hifi_serial
