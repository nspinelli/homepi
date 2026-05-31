#include "homepi/hifi-serial/config-loader.hpp"

#include <cstdlib>
#include <fstream>
#include <sstream>

#include "homepi/hifi-serial/json-utils.hpp"

namespace homepi::hifi_serial {

namespace {

std::string read_file(const std::string& path) {
  std::ifstream input(path);
  std::ostringstream buffer;
  buffer << input.rdbuf();
  return buffer.str();
}

std::string env_or(const char* name, const std::string& fallback) {
  if (const char* value = std::getenv(name)) {
    if (value[0] != '\0') {
      return value;
    }
  }
  return fallback;
}

}  // namespace

ServiceConfig load_service_config(const std::string& config_path) {
  ServiceConfig config;
  const std::string json = read_file(config_path);

  config.service = json_get_string(json, "service");
  if (config.service.empty()) {
    config.service = "homepi-hifi-serial";
  }

  config.generated_dir = json_get_string(json, "generatedDir");
  config.state_dir = json_get_string(json, "stateDir");
  config.socket_dir = json_get_string(json, "socketDir");

  if (config.generated_dir.empty()) {
    config.generated_dir = env_or("HOMEPI_GENERATED_DIR", "/opt/homepi/runtime/generated");
  }
  if (config.state_dir.empty()) {
    config.state_dir = env_or("HOMEPI_STATE_DIR", "/opt/homepi/runtime/state");
  }
  if (config.socket_dir.empty()) {
    config.socket_dir = env_or("HOMEPI_SOCKET_DIR", "/run/homepi");
  }

  config.database_path = json_get_string(json, "databasePath");
  const std::string state_override = env_or("HOMEPI_STATE_DIR", "");
  if (!state_override.empty()) {
    config.state_dir = state_override;
    config.database_path = state_override + "/homepi.sqlite";
  } else if (config.database_path.empty()) {
    config.database_path = config.state_dir + "/homepi.sqlite";
  }

  config.socket_path = json_get_string(json, "socketPath");
  const std::string socket_override = env_or("HOMEPI_SOCKET_DIR", "");
  if (!socket_override.empty() && config.socket_path.empty()) {
    config.socket_path = socket_override + "/hifi-serial.sock";
  } else if (config.socket_path.empty()) {
    config.socket_path = config.socket_dir + "/hifi-serial.sock";
  }

  config.virtual_port = json_get_string(json, "virtualPort");
  if (config.virtual_port.empty()) {
    config.virtual_port = "/dev/vHifi";
  }

  const std::string level = json_get_string(json, "level");
  if (!level.empty()) {
    config.log_level = level;
  }

  const int baud_rate = json_get_int(json, "baudRate");
  if (baud_rate > 0) {
    config.baud_rate = baud_rate;
  }

  const int command_interval_ms = json_get_int(json, "commandIntervalMs");
  if (command_interval_ms > 0) {
    config.command_interval_ms = command_interval_ms;
  }

  return config;
}

}  // namespace homepi::hifi_serial
