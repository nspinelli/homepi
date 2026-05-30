#include "homepi/shairport-sync/config-loader.hpp"

#include <cstdlib>
#include <fstream>
#include <sstream>

#include "homepi/shairport-sync/json-utils.hpp"

namespace homepi::shairport_sync {

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

int json_get_int(const std::string& json, const char* field, int fallback) {
  const std::string key = std::string("\"") + field + "\"";
  const auto pos = json.find(key);
  if (pos == std::string::npos) {
    return fallback;
  }
  const auto colon = json.find(':', pos);
  if (colon == std::string::npos) {
    return fallback;
  }
  try {
    return std::stoi(json.substr(colon + 1));
  } catch (...) {
    return fallback;
  }
}

}  // namespace

ServiceConfig load_service_config(const std::string& config_path) {
  ServiceConfig config;
  const std::string json = read_file(config_path);

  config.service = json_get_string(json, "service");
  if (config.service.empty()) {
    config.service = "homepi-shairport-supervisor";
  }

  const auto logging_pos = json.find("\"logging\"");
  if (logging_pos != std::string::npos) {
    const std::string level = json_get_string(json.substr(logging_pos), "level");
    if (!level.empty()) {
      config.log_level = level;
    }
  }

  const auto shairport_pos = json.find("\"shairportSync\"");
  const std::string shairport_section =
      shairport_pos != std::string::npos ? json.substr(shairport_pos) : json;

  config.state_dir = env_or("HOMEPI_STATE_DIR", "/opt/homepi/runtime/state");
  config.socket_dir = env_or("HOMEPI_SOCKET_DIR", "/run/homepi");
  config.database_path = config.state_dir + "/homepi.sqlite";

  const auto runtime_pos = json.find("\"runtime\"");
  if (runtime_pos != std::string::npos) {
    const std::string runtime_section = json.substr(runtime_pos);
    const std::string db_path = json_get_string(runtime_section, "databasePath");
    if (!db_path.empty()) {
      config.database_path = db_path;
    }
  }

  config.install_root = json_get_string(shairport_section, "installRoot");
  if (config.install_root.empty()) {
    config.install_root = env_or("HOMEPI_SHAIRPORT_ROOT", "/opt/homepi/services/shairport");
  }

  config.zones_config_dir = config.install_root + "/config/zones";
  config.hooks_dir = config.install_root + "/bin/hooks";
  config.shairport_binary = config.install_root + "/bin/shairport-sync";
  config.hifi_socket_path = config.socket_dir + "/hifi-serial.sock";
  config.pcm_router_socket_path = config.socket_dir + "/pcm-router.sock";
  config.supervisor_socket_path = config.socket_dir + "/shairport-supervisor.sock";

  config.upstream_version = json_get_string(shairport_section, "upstreamVersion");
  if (config.upstream_version.empty()) {
    config.upstream_version = "4.3.6";
  }

  config.health_interval_sec = json_get_int(shairport_section, "healthIntervalSec", 30);
  config.zone_count = json_get_int(shairport_section, "zoneCount", 16);

  return config;
}

}  // namespace homepi::shairport_sync
