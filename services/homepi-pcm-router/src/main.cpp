#include "homepi/pcm-router/service.hpp"

#include <cstdlib>
#include <iostream>
#include <string>

namespace {

homepi::pcm_router::ServiceConfig load_config_from_env() {
  homepi::pcm_router::ServiceConfig config;
  if (const char* value = std::getenv("HOMEPI_EVENT_SOCKET")) {
    config.socket_path = value;
  }
  if (const char* value = std::getenv("HOMEPI_DATABASE_PATH")) {
    config.database_path = value;
  }
  if (const char* value = std::getenv("HOMEPI_GENERATED_DIR")) {
    config.artifact_path = std::string(value) + "/audio/operating-profile.json";
  }
  if (const char* value = std::getenv("HOMEPI_USB_DEVICES_SOCKET")) {
    config.usb_devices_socket = value;
  }
  if (const char* value = std::getenv("HOMEPI_EVENTS_SOCKET")) {
    config.events_socket = value;
  }
  if (const char* value = std::getenv("LOG_LEVEL")) {
    config.log_level = value;
  }
  if (const char* value = std::getenv("HOMEPI_ZONE_COUNT")) {
    config.zone_count = std::atoi(value);
  }
  if (const char* value = std::getenv("PERIOD_FRAMES")) {
    config.period_frames = static_cast<uint32_t>(std::atoi(value));
  }
  if (const char* value = std::getenv("BUFFER_FRAMES")) {
    config.buffer_frames = static_cast<uint32_t>(std::atoi(value));
  }
  if (const char* cards = std::getenv("ALSA_LOOPBACK_CARDS")) {
    std::string text = cards;
    const auto comma = text.find(',');
    if (comma != std::string::npos) {
      config.loopback_card_a = text.substr(0, comma);
      config.loopback_card_b = text.substr(comma + 1);
    } else {
      config.loopback_card_a = text;
    }
  }
  return config;
}

}

int main() {
  homepi::pcm_router::Service service(load_config_from_env());
  return service.run();
}
