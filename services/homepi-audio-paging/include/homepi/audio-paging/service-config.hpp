#pragma once

#include <string>

namespace homepi::audio_paging {

/** Runtime configuration loaded from service-config.json and environment overrides. */
struct ServiceConfig {
  std::string service = "homepi-audio-paging";
  std::string events_socket = "/run/homepi/broker/broker.sock";
  std::string socket_path = "/run/homepi/audio/paging.sock";
  std::string socket_dir = "/run/homepi";
  std::string usb_devices_socket = "/run/homepi/usb/usb.sock";
  std::string hifi_serial_socket = "/run/homepi/audio/hifi-serial.sock";
  std::string database_path = "/opt/homepi/runtime/state/homepi.sqlite";
  std::string state_dir = "/opt/homepi/runtime/state";
  std::string asset_root = "/opt/homepi/services/audio-paging/assets";
  std::string voices_root = "/var/lib/homepi/paging/voices";
  std::string chimes_root = "/var/lib/homepi/paging/chimes";
  std::string piper_binary = "piper";
  std::string piper_default_voice = "/var/lib/homepi/paging/voices/en_US-lessac-medium.onnx";
  std::string piper_default_config =
      "/var/lib/homepi/paging/voices/en_US-lessac-medium.onnx.json";
  std::string alsa_device = "plug:AudioPaging";
  std::string log_level = "INFO";
  bool always_warm_default = true;
  int page_on_timeout_ms = 2000;
  int page_off_timeout_ms = 2000;
  int piper_warm_timeout_ms = 8000;
  int tts_generation_timeout_ms = 10000;
};

/**
 * Loads service configuration from a JSON file.
 * @param config_path Absolute or relative file path.
 * @return Parsed service configuration with defaults.
 */
ServiceConfig load_service_config(const std::string& config_path);

}  // namespace homepi::audio_paging
