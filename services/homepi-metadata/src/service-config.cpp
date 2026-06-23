#include "homepi/metadata/service-config.hpp"

#include <cstdlib>

namespace homepi::metadata {

ServiceConfig load_config_from_env() {
  ServiceConfig config;
  if (const char* value = std::getenv("HOMEPI_EVENT_SOCKET")) {
    config.socket_path = value;
  }
  if (const char* value = std::getenv("HOMEPI_EVENTS_SOCKET")) {
    config.events_socket = value;
  }
  if (const char* value = std::getenv("HOMEPI_AUDIO_REALTIME_SOCKET")) {
    config.realtime_socket_path = value;
  }
  if (const char* value = std::getenv("HOMEPI_DATABASE_PATH")) {
    config.database_path = value;
  }
  if (const char* value = std::getenv("HOMEPI_CACHE_DIR")) {
    config.cache_dir = value;
  }
  if (const char* value = std::getenv("HOMEPI_METADATA_PIPE_PREFIX")) {
    config.pipe_prefix = value;
  }
  if (const char* value = std::getenv("LOG_LEVEL")) {
    config.log_level = value;
  }
  if (const char* value = std::getenv("HOMEPI_ZONE_COUNT")) {
    config.zone_count = std::atoi(value);
  }
  if (const char* value = std::getenv("METADATA_DEBOUNCE_MS")) {
    config.metadata_debounce_ms = std::atoi(value);
  }
  return config;
}

}  // namespace homepi::metadata
