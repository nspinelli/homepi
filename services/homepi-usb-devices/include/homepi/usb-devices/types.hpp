#pragma once

#include <optional>
#include <string>
#include <vector>

namespace homepi::usb_devices {

/** USB device capability kind. */
enum class DeviceKind { Serial, Audio };

/**
 * Converts a device kind to its schema string.
 * @param kind Device kind.
 * @return "serial" or "audio".
 */
inline const char* device_kind_to_string(DeviceKind kind) {
  return kind == DeviceKind::Serial ? "serial" : "audio";
}

/** Detected or persisted USB device record. */
struct UsbDevice {
  std::string device_id;
  std::string display_name;
  DeviceKind kind = DeviceKind::Serial;
  bool present = false;
  std::string id_vendor;
  std::string id_product;
  std::string serial;
  std::string devpath;
  std::string alsa_card;
  std::string resolved_alsa_name;
};

/** Role assignments referencing device IDs. */
struct UsbAssignments {
  std::optional<std::string> serial;
  std::optional<std::string> audio_primary;
  std::optional<std::string> paging;
};

/** Service runtime paths and options. */
struct ServiceConfig {
  std::string service = "homepi-usb-devices";
  std::string log_level = "INFO";
  std::string generated_dir = "/opt/homepi/runtime/generated";
  std::string state_dir = "/opt/homepi/runtime/state";
  std::string socket_dir = "/run/homepi";
  std::string database_path = "/opt/homepi/runtime/state/homepi.sqlite";
  std::string socket_path = "/run/homepi/usb-devices.sock";
  std::string serial_symlink = "vHifi";
  std::string udev_rules_relative = "udev/99-homepi-usb-devices.rules";
  /** Stable ALSA card ID for primary audio (pcm-router). */
  std::string primary_audio_alsa_id = "HomePiPrimaryAudio";
  /** Reserved snd-usb-audio index for primary DAC (loopback uses 10–11). */
  int primary_audio_modprobe_index = 2;
  std::string modprobe_relative = "modprobe/homepi-audio-primary.conf";
};

/** Daemon health snapshot for API consumers. */
struct ServiceHealth {
  std::string lifecycle = "running";
  bool udev_monitor_active = false;
  int connected_device_count = 0;
  bool assignments_degraded = false;
  std::string last_scan_at;
};

}  // namespace homepi::usb_devices
