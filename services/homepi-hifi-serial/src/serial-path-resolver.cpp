#include "homepi/hifi-serial/serial-path-resolver.hpp"

#include <filesystem>
#include <sqlite3.h>

namespace fs = std::filesystem;

namespace homepi::hifi_serial {

namespace {

bool path_exists(const std::string& path) {
  return !path.empty() && fs::exists(path);
}

void persist_serial_path(sqlite3* db, const std::string& path, const std::string& device_id) {
  if (path.empty() || db == nullptr) {
    return;
  }
  sqlite3_stmt* stmt = nullptr;
  const char* sql =
      "UPDATE hifi_controller SET serial_path = ?, serial_device_id = ?, "
      "updated_at = strftime('%Y-%m-%dT%H:%M:%SZ', 'now') WHERE id = 1";
  if (sqlite3_prepare_v2(db, sql, -1, &stmt, nullptr) == SQLITE_OK) {
    sqlite3_bind_text(stmt, 1, path.c_str(), -1, SQLITE_TRANSIENT);
    if (device_id.empty()) {
      sqlite3_bind_null(stmt, 2);
    } else {
      sqlite3_bind_text(stmt, 2, device_id.c_str(), -1, SQLITE_TRANSIENT);
    }
    sqlite3_step(stmt);
    sqlite3_finalize(stmt);
  }
}

SerialPathResolution read_hifi_controller_serial(sqlite3* db) {
  SerialPathResolution result;
  sqlite3_stmt* stmt = nullptr;
  const char* sql = "SELECT serial_path, serial_device_id FROM hifi_controller WHERE id = 1";
  if (sqlite3_prepare_v2(db, sql, -1, &stmt, nullptr) != SQLITE_OK) {
    return result;
  }
  if (sqlite3_step(stmt) == SQLITE_ROW) {
    if (sqlite3_column_type(stmt, 0) != SQLITE_NULL) {
      result.path = reinterpret_cast<const char*>(sqlite3_column_text(stmt, 0));
    }
    if (sqlite3_column_type(stmt, 1) != SQLITE_NULL) {
      result.device_id = reinterpret_cast<const char*>(sqlite3_column_text(stmt, 1));
    }
  }
  sqlite3_finalize(stmt);
  return result;
}

SerialPathResolution read_usb_assignment_serial(sqlite3* db) {
  SerialPathResolution result;
  sqlite3_stmt* stmt = nullptr;
  const char* sql =
      "SELECT a.serial_device_id, d.devpath FROM usb_assignments a "
      "LEFT JOIN usb_devices d ON d.device_id = a.serial_device_id "
      "WHERE a.id = 1";
  if (sqlite3_prepare_v2(db, sql, -1, &stmt, nullptr) != SQLITE_OK) {
    return result;
  }
  if (sqlite3_step(stmt) == SQLITE_ROW) {
    if (sqlite3_column_type(stmt, 0) != SQLITE_NULL) {
      result.device_id = reinterpret_cast<const char*>(sqlite3_column_text(stmt, 0));
    }
    if (sqlite3_column_type(stmt, 1) != SQLITE_NULL) {
      result.path = reinterpret_cast<const char*>(sqlite3_column_text(stmt, 1));
    }
  }
  sqlite3_finalize(stmt);
  return result;
}

}  // namespace

SerialPathResolution resolve_serial_path(const std::string& database_path,
                                         const std::string& virtual_port) {
  sqlite3* db = nullptr;
  if (sqlite3_open(database_path.c_str(), &db) != SQLITE_OK) {
    return {};
  }
  sqlite3_busy_timeout(db, 5000);

  SerialPathResolution stored = read_hifi_controller_serial(db);
  if (path_exists(stored.path)) {
    sqlite3_close(db);
    return stored;
  }

  if (!stored.path.empty() && stored.path == virtual_port) {
    SerialPathResolution tty_fallback = read_usb_assignment_serial(db);
    if (path_exists(tty_fallback.path)) {
      sqlite3_close(db);
      return tty_fallback;
    }
  }

  if (path_exists(virtual_port)) {
    SerialPathResolution device_id_source = stored.device_id.empty() ? read_usb_assignment_serial(db) : stored;
    persist_serial_path(db, virtual_port, device_id_source.device_id);
    sqlite3_close(db);
    return {.path = virtual_port, .device_id = device_id_source.device_id};
  }

  if (!stored.path.empty()) {
    SerialPathResolution tty_fallback = read_usb_assignment_serial(db);
    if (path_exists(tty_fallback.path)) {
      sqlite3_close(db);
      return tty_fallback;
    }
  }

  SerialPathResolution from_usb = read_usb_assignment_serial(db);
  if (path_exists(from_usb.path)) {
    persist_serial_path(db, from_usb.path, from_usb.device_id);
    sqlite3_close(db);
    return from_usb;
  }

  sqlite3_close(db);
  return {};
}

}  // namespace homepi::hifi_serial
