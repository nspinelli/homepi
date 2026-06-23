#include "homepi/audio-orchestrator/service-config.hpp"

#include <cstdlib>
#include <fstream>
#include <sstream>

namespace homepi::audio_orchestrator {

namespace {

std::string read_json_string_field(const std::string& json, const std::string& key) {
  const std::string needle = "\"" + key + "\"";
  const auto key_pos = json.find(needle);
  if (key_pos == std::string::npos) {
    return {};
  }
  const auto colon = json.find(':', key_pos);
  if (colon == std::string::npos) {
    return {};
  }
  const auto quote_start = json.find('"', colon);
  if (quote_start == std::string::npos) {
    return {};
  }
  const auto quote_end = json.find('"', quote_start + 1);
  if (quote_end == std::string::npos) {
    return {};
  }
  return json.substr(quote_start + 1, quote_end - quote_start - 1);
}

int read_json_int_field(const std::string& json, const std::string& key) {
  const std::string needle = "\"" + key + "\"";
  const auto key_pos = json.find(needle);
  if (key_pos == std::string::npos) {
    return 0;
  }
  const auto colon = json.find(':', key_pos);
  if (colon == std::string::npos) {
    return 0;
  }
  return std::atoi(json.c_str() + colon + 1);
}

std::string env_or_empty(const char* name) {
  if (const char* value = std::getenv(name)) {
    return value;
  }
  return {};
}

}  // namespace

ServiceConfig load_service_config(const std::string& config_path) {
  ServiceConfig config;
  std::ifstream in(config_path);
  if (in) {
    std::ostringstream buffer;
    buffer << in.rdbuf();
    const std::string json = buffer.str();
    if (const std::string service = read_json_string_field(json, "service"); !service.empty()) {
      config.service = service;
    }
    if (const std::string events_socket = read_json_string_field(json, "eventsSocket");
        !events_socket.empty()) {
      config.events_socket = events_socket;
    }
    if (const std::string pcm_socket = read_json_string_field(json, "pcmRouterSocket");
        !pcm_socket.empty()) {
      config.pcm_router_socket = pcm_socket;
    }
    if (const std::string hifi_socket = read_json_string_field(json, "hifiSerialSocket");
        !hifi_socket.empty()) {
      config.hifi_serial_socket = hifi_socket;
    }
    if (const std::string db_path = read_json_string_field(json, "databasePath"); !db_path.empty()) {
      config.database_path = db_path;
    }
    if (const std::string nqptp_host = read_json_string_field(json, "host"); !nqptp_host.empty()) {
      config.nqptp_host = nqptp_host;
    }
    const int nqptp_port = read_json_int_field(json, "port");
    if (nqptp_port > 0) {
      config.nqptp_port = nqptp_port;
    }
    const int default_source = read_json_int_field(json, "airplaySource");
    if (default_source > 0) {
      config.default_airplay_source = default_source;
    }
  }

  if (const std::string value = env_or_empty("HOMEPI_EVENTS_SOCKET"); !value.empty()) {
    config.events_socket = value;
  }
  if (const std::string value = env_or_empty("HOMEPI_PCM_ROUTER_SOCKET"); !value.empty()) {
    config.pcm_router_socket = value;
  }
  if (const std::string value = env_or_empty("HOMEPI_HIFI_SOCKET"); !value.empty()) {
    config.hifi_serial_socket = value;
  }
  if (const std::string value = env_or_empty("HOMEPI_DATABASE_PATH"); !value.empty()) {
    config.database_path = value;
  }
  if (const std::string value = env_or_empty("AIRPLAY_SOURCE"); !value.empty()) {
    config.default_airplay_source = std::atoi(value.c_str());
  }

  return config;
}

}  // namespace homepi::audio_orchestrator
