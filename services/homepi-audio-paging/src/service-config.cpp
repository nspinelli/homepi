#include "homepi/audio-paging/service-config.hpp"

#include <cstdlib>
#include <fstream>
#include <sstream>

#include "homepi/audio-paging/json-utils.hpp"

namespace homepi::audio_paging {

namespace {

std::string read_file(const std::string& path) {
  std::ifstream input(path);
  std::ostringstream out;
  out << input.rdbuf();
  return out.str();
}

std::string env_or(const char* key, const std::string& fallback) {
  if (const char* value = std::getenv(key)) {
    if (value[0] != '\0') {
      return value;
    }
  }
  return fallback;
}

int parse_int_or(const std::string& value, int fallback) {
  if (value.empty()) {
    return fallback;
  }
  try {
    return std::stoi(value);
  } catch (...) {
    return fallback;
  }
}

bool parse_bool_or(const std::string& value, bool fallback) {
  if (value == "true" || value == "1") {
    return true;
  }
  if (value == "false" || value == "0") {
    return false;
  }
  return fallback;
}

}  // namespace

ServiceConfig load_service_config(const std::string& config_path) {
  ServiceConfig config;
  const std::string json = read_file(config_path);
  const std::string audio_paging = json_get_object(json, "audioPaging");
  const std::string source = audio_paging.empty() ? json : audio_paging;

  const std::string service = json_get_string(json, "service");
  if (!service.empty()) {
    config.service = service;
  }

  const std::string events_socket = json_get_string(source, "eventsSocket");
  if (!events_socket.empty()) {
    config.events_socket = events_socket;
  }
  const std::string socket_path = json_get_string(source, "socketPath");
  if (!socket_path.empty()) {
    config.socket_path = socket_path;
  }
  const std::string usb_socket = json_get_string(source, "usbDevicesSocket");
  if (!usb_socket.empty()) {
    config.usb_devices_socket = usb_socket;
  }
  const std::string hifi_socket = json_get_string(source, "hifiSerialSocket");
  if (!hifi_socket.empty()) {
    config.hifi_serial_socket = hifi_socket;
  }
  const std::string database_path = json_get_string(source, "databasePath");
  if (!database_path.empty()) {
    config.database_path = database_path;
  }
  const std::string state_dir = json_get_string(source, "stateDir");
  if (!state_dir.empty()) {
    config.state_dir = state_dir;
  }
  const std::string asset_root = json_get_string(source, "assetRoot");
  if (!asset_root.empty()) {
    config.asset_root = asset_root;
  }
  const std::string voices_root = json_get_string(source, "voicesRoot");
  if (!voices_root.empty()) {
    config.voices_root = voices_root;
  }
  const std::string chimes_root = json_get_string(source, "chimesRoot");
  if (!chimes_root.empty()) {
    config.chimes_root = chimes_root;
  }
  const std::string piper_binary = json_get_string(source, "piperBinary");
  if (!piper_binary.empty()) {
    config.piper_binary = piper_binary;
  }
  const std::string piper_default_voice = json_get_string(source, "piperDefaultVoice");
  if (!piper_default_voice.empty()) {
    config.piper_default_voice = piper_default_voice;
  }
  const std::string piper_default_config = json_get_string(source, "piperDefaultConfig");
  if (!piper_default_config.empty()) {
    config.piper_default_config = piper_default_config;
  }
  const std::string alsa_device = json_get_string(source, "alsaDevice");
  if (!alsa_device.empty()) {
    config.alsa_device = alsa_device;
  }
  const std::string log_level = json_get_string(json, "level");
  if (log_level.empty()) {
    const std::string nested_level = json_get_string(json_get_object(json, "logging"), "level");
    if (!nested_level.empty()) {
      config.log_level = nested_level;
    }
  } else {
    config.log_level = log_level;
  }
  config.always_warm_default =
      parse_bool_or(json_get_scalar(source, "alwaysWarmDefault"), config.always_warm_default);
  config.page_on_timeout_ms =
      parse_int_or(json_get_scalar(source, "pageOnTimeoutMs"), config.page_on_timeout_ms);
  config.page_off_timeout_ms =
      parse_int_or(json_get_scalar(source, "pageOffTimeoutMs"), config.page_off_timeout_ms);
  config.piper_warm_timeout_ms =
      parse_int_or(json_get_scalar(source, "piperWarmTimeoutMs"), config.piper_warm_timeout_ms);
  config.tts_generation_timeout_ms = parse_int_or(json_get_scalar(source, "ttsGenerationTimeoutMs"),
                                                    config.tts_generation_timeout_ms);

  config.events_socket = env_or("HOMEPI_EVENTS_SOCKET", config.events_socket);
  config.database_path = env_or("HOMEPI_DATABASE_PATH", config.database_path);
  config.socket_path = env_or("HOMEPI_PAGING_SOCKET", config.socket_path);
  config.usb_devices_socket = env_or("HOMEPI_USB_DEVICES_SOCKET", config.usb_devices_socket);
  config.hifi_serial_socket = env_or("HOMEPI_HIFI_SERIAL_SOCKET", config.hifi_serial_socket);
  config.state_dir = env_or("HOMEPI_STATE_DIR", config.state_dir);
  config.asset_root = env_or("HOMEPI_AUDIO_PAGING_ASSET_ROOT", config.asset_root);
  config.voices_root = env_or("HOMEPI_AUDIO_PAGING_VOICES_ROOT", config.voices_root);
  config.chimes_root = env_or("HOMEPI_AUDIO_PAGING_CHIMES_ROOT", config.chimes_root);
  config.piper_binary = env_or("HOMEPI_PIPER_BIN", config.piper_binary);
  config.alsa_device = env_or("HOMEPI_AUDIO_PAGING_ALSA_DEVICE", config.alsa_device);
  config.log_level = env_or("LOG_LEVEL", config.log_level);

  const auto slash = config.socket_path.find_last_of('/');
  config.socket_dir = slash == std::string::npos ? "/run/homepi" : config.socket_path.substr(0, slash);
  return config;
}

}  // namespace homepi::audio_paging
