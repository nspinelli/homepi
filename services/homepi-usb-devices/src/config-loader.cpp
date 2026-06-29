#include "homepi/usb-devices/config-loader.hpp"

#include <cstdlib>
#include <fstream>
#include <sstream>

#include "homepi/usb-devices/json-utils.hpp"

namespace homepi::usb_devices {

namespace {

/**
 * Reads an entire file into a string.
 * @param path File path.
 * @return File contents.
 */
std::string read_file(const std::string& path) {
  std::ifstream input(path);
  std::ostringstream buffer;
  buffer << input.rdbuf();
  return buffer.str();
}

/**
 * Returns an environment variable or a default value.
 * @param name Variable name.
 * @param fallback Default value.
 * @return Resolved value.
 */
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
    config.service = "homepi-usb-devices";
  }

  const std::string runtime_block = json;
  config.generated_dir = json_get_string(runtime_block, "generatedDir");
  config.state_dir = json_get_string(runtime_block, "stateDir");
  config.socket_dir = json_get_string(runtime_block, "socketDir");

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

  const std::string generated_override = env_or("HOMEPI_GENERATED_DIR", "");
  if (!generated_override.empty()) {
    config.generated_dir = generated_override;
  }

  const std::string socket_override = env_or("HOMEPI_SOCKET_DIR", "");
  if (!socket_override.empty()) {
    config.socket_dir = socket_override;
  }

  config.socket_path = json_get_string(json, "socketPath");
  if (!socket_override.empty()) {
    config.socket_path = socket_override + "/usb/usb.sock";
  } else if (config.socket_path.empty()) {
    config.socket_path = config.socket_dir + "/usb/usb.sock";
  }

  config.serial_symlink = json_get_string(json, "serialSymlink");
  if (config.serial_symlink.empty()) {
    config.serial_symlink = "vHifi";
  }

  const std::string rules_file = json_get_string(json, "udevRulesFile");
  if (!rules_file.empty()) {
    config.udev_rules_relative = rules_file;
  }

  const std::string level = json_get_string(json, "level");
  if (!level.empty()) {
    config.log_level = level;
  }

  return config;
}

}  // namespace homepi::usb_devices
