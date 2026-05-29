#pragma once

#include <string>

namespace homepi::hifi_serial {

/** Resolved serial device path and optional device id. */
struct SerialPathResolution {
  std::string path;
  std::string device_id;
};

/**
 * Resolves the serial device path from hifi_controller (authoritative) and config.
 * Does not call homepi-usb-devices; may read shared SQLite usb_* tables only to seed
 * hifi_controller when serial_path is unset.
 * @param database_path SQLite database path.
 * @param virtual_port Configured symlink path (e.g. /dev/vHifi).
 * @return Resolved path and device id (empty path when unavailable).
 */
SerialPathResolution resolve_serial_path(const std::string& database_path,
                                         const std::string& virtual_port);

}  // namespace homepi::hifi_serial
