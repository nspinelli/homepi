#include "homepi/usb-devices/assignment-repository.hpp"

#include <algorithm>
#include <chrono>
#include <ctime>
#include <filesystem>
#include <optional>
#include <set>
#include <sqlite3.h>
#include <sstream>
#include <stdexcept>

#include "homepi/storage/audio-profile-types.hpp"
#include "homepi/usb-devices/device-id.hpp"

namespace homepi::usb_devices {

namespace {

/**
 * Returns current UTC ISO8601 timestamp.
 * @return Timestamp string.
 */
std::string utc_now() {
  const auto now = std::chrono::system_clock::now();
  const std::time_t t = std::chrono::system_clock::to_time_t(now);
  std::tm tm{};
  gmtime_r(&t, &tm);
  char buffer[32];
  std::strftime(buffer, sizeof(buffer), "%Y-%m-%dT%H:%M:%SZ", &tm);
  return buffer;
}

/**
 * Mirrors USB serial assignment into hifi_controller for homepi-hifi-serial.
 * @param db SQLite connection.
 * @param assignments Saved assignments.
 * @param devices Current device list.
 */
void sync_hifi_controller_serial(sqlite3* db, const UsbAssignments& assignments,
                                 const std::vector<UsbDevice>& devices) {
  const std::string now = utc_now();
  std::string device_id;
  std::string devpath;

  if (assignments.serial && !assignments.serial->empty()) {
    device_id = *assignments.serial;
    for (const UsbDevice& device : devices) {
      if (device.device_id == device_id) {
        devpath = device.devpath;
        break;
      }
    }
  }

  const std::string virtual_port = "/dev/vHifi";
  std::string serial_path = virtual_port;
  if (!std::filesystem::exists(virtual_port) && !devpath.empty()) {
    serial_path = devpath;
  }

  const char* sql =
      "UPDATE hifi_controller SET serial_device_id = ?, serial_path = ?, updated_at = ? "
      "WHERE id = 1";
  sqlite3_stmt* stmt = nullptr;
  if (sqlite3_prepare_v2(db, sql, -1, &stmt, nullptr) != SQLITE_OK) {
    return;
  }
  if (device_id.empty()) {
    sqlite3_bind_null(stmt, 1);
  } else {
    sqlite3_bind_text(stmt, 1, device_id.c_str(), -1, SQLITE_TRANSIENT);
  }
  if (serial_path.empty()) {
    sqlite3_bind_null(stmt, 2);
  } else {
    sqlite3_bind_text(stmt, 2, serial_path.c_str(), -1, SQLITE_TRANSIENT);
  }
  sqlite3_bind_text(stmt, 3, now.c_str(), -1, SQLITE_TRANSIENT);
  sqlite3_step(stmt);
  sqlite3_finalize(stmt);
}

DeviceKind parse_kind(const std::string& kind) {
  return kind == "audio" ? DeviceKind::Audio : DeviceKind::Serial;
}

}  // namespace

AssignmentRepository::AssignmentRepository(const std::string& database_path,
                                         const std::string& migrations_sql) {
  sqlite3* raw = nullptr;
  if (sqlite3_open(database_path.c_str(), &raw) != SQLITE_OK) {
    throw std::runtime_error("Failed to open sqlite database: " + database_path);
  }
  db_ = raw;
  sqlite3_busy_timeout(static_cast<sqlite3*>(db_), 10000);
  char* err = nullptr;
  if (sqlite3_exec(static_cast<sqlite3*>(db_), migrations_sql.c_str(), nullptr, nullptr, &err) !=
      SQLITE_OK) {
    std::string message = err != nullptr ? err : "migration failed";
    sqlite3_free(err);
    throw std::runtime_error(message);
  }
}

AssignmentRepository::~AssignmentRepository() {
  if (db_ != nullptr) {
    sqlite3_close(static_cast<sqlite3*>(db_));
    db_ = nullptr;
  }
}

void AssignmentRepository::upsert_devices(const std::vector<UsbDevice>& scanned) {
  auto* db = static_cast<sqlite3*>(db_);
  const std::string now = utc_now();

  sqlite3_exec(db, "UPDATE usb_devices SET present = 0", nullptr, nullptr, nullptr);

  const char* upsert_sql =
      "INSERT INTO usb_devices "
      "(device_id, display_name, kind, id_vendor, id_product, serial, devpath, alsa_card, present, "
      "updated_at) "
      "VALUES (?, ?, ?, ?, ?, ?, ?, ?, 1, ?) "
      "ON CONFLICT(device_id) DO UPDATE SET "
      "display_name=excluded.display_name, kind=excluded.kind, id_vendor=excluded.id_vendor, "
      "id_product=excluded.id_product, serial=excluded.serial, devpath=excluded.devpath, "
      "alsa_card=excluded.alsa_card, present=1, updated_at=excluded.updated_at";

  sqlite3_stmt* stmt = nullptr;
  sqlite3_prepare_v2(db, upsert_sql, -1, &stmt, nullptr);
  for (const UsbDevice& device : scanned) {
    sqlite3_bind_text(stmt, 1, device.device_id.c_str(), -1, SQLITE_TRANSIENT);
    sqlite3_bind_text(stmt, 2, device.display_name.c_str(), -1, SQLITE_TRANSIENT);
    sqlite3_bind_text(stmt, 3, device_kind_to_string(device.kind), -1, SQLITE_TRANSIENT);
    sqlite3_bind_text(stmt, 4, device.id_vendor.c_str(), -1, SQLITE_TRANSIENT);
    sqlite3_bind_text(stmt, 5, device.id_product.c_str(), -1, SQLITE_TRANSIENT);
    sqlite3_bind_text(stmt, 6, device.serial.c_str(), -1, SQLITE_TRANSIENT);
    sqlite3_bind_text(stmt, 7, device.devpath.c_str(), -1, SQLITE_TRANSIENT);
    sqlite3_bind_text(stmt, 8, device.alsa_card.c_str(), -1, SQLITE_TRANSIENT);
    sqlite3_bind_text(stmt, 9, now.c_str(), -1, SQLITE_TRANSIENT);
    sqlite3_step(stmt);
    sqlite3_reset(stmt);
  }
  sqlite3_finalize(stmt);

  sqlite3_exec(db, "DELETE FROM usb_devices WHERE present = 0", nullptr, nullptr, nullptr);
}

std::vector<UsbDevice> AssignmentRepository::list_devices() const {
  std::vector<UsbDevice> devices;
  auto* db = static_cast<sqlite3*>(db_);
  const char* sql =
      "SELECT device_id, display_name, kind, id_vendor, id_product, serial, devpath, alsa_card, "
      "present FROM usb_devices WHERE present = 1 ORDER BY display_name";
  sqlite3_stmt* stmt = nullptr;
  sqlite3_prepare_v2(db, sql, -1, &stmt, nullptr);
  while (sqlite3_step(stmt) == SQLITE_ROW) {
    UsbDevice device;
    device.device_id = reinterpret_cast<const char*>(sqlite3_column_text(stmt, 0));
    device.display_name = reinterpret_cast<const char*>(sqlite3_column_text(stmt, 1));
    device.kind = parse_kind(reinterpret_cast<const char*>(sqlite3_column_text(stmt, 2)));
    if (const char* vendor = reinterpret_cast<const char*>(sqlite3_column_text(stmt, 3))) {
      device.id_vendor = vendor;
    }
    if (const char* product = reinterpret_cast<const char*>(sqlite3_column_text(stmt, 4))) {
      device.id_product = product;
    }
    if (const char* serial = reinterpret_cast<const char*>(sqlite3_column_text(stmt, 5))) {
      device.serial = serial;
    }
    if (const char* devpath = reinterpret_cast<const char*>(sqlite3_column_text(stmt, 6))) {
      device.devpath = devpath;
    }
    if (const char* alsa = reinterpret_cast<const char*>(sqlite3_column_text(stmt, 7))) {
      device.alsa_card = alsa;
      device.resolved_alsa_name = "hw:" + device.alsa_card + ",0";
    }
    device.present = sqlite3_column_int(stmt, 8) != 0;
    devices.push_back(std::move(device));
  }
  sqlite3_finalize(stmt);
  return devices;
}

std::optional<UsbDevice> AssignmentRepository::get_device(const std::string& device_id) const {
  for (const UsbDevice& device : list_devices()) {
    if (device.device_id == device_id) {
      return device;
    }
  }
  return std::nullopt;
}

void AssignmentRepository::set_device_present(const std::string& device_id, bool present) {
  auto* db = static_cast<sqlite3*>(db_);
  const char* sql = "UPDATE usb_devices SET present = ?, updated_at = ? WHERE device_id = ?";
  sqlite3_stmt* stmt = nullptr;
  sqlite3_prepare_v2(db, sql, -1, &stmt, nullptr);
  const std::string now = utc_now();
  sqlite3_bind_int(stmt, 1, present ? 1 : 0);
  sqlite3_bind_text(stmt, 2, now.c_str(), -1, SQLITE_TRANSIENT);
  sqlite3_bind_text(stmt, 3, device_id.c_str(), -1, SQLITE_TRANSIENT);
  sqlite3_step(stmt);
  sqlite3_finalize(stmt);
}

UsbAssignments AssignmentRepository::get_assignments() const {
  UsbAssignments assignments;
  auto* db = static_cast<sqlite3*>(db_);
  const char* sql =
      "SELECT serial_device_id, audio_primary_device_id, paging_device_id FROM usb_assignments "
      "WHERE id = 1";
  sqlite3_stmt* stmt = nullptr;
  sqlite3_prepare_v2(db, sql, -1, &stmt, nullptr);
  if (sqlite3_step(stmt) == SQLITE_ROW) {
    if (const char* value = reinterpret_cast<const char*>(sqlite3_column_text(stmt, 0))) {
      assignments.serial = value;
    }
    if (const char* value = reinterpret_cast<const char*>(sqlite3_column_text(stmt, 1))) {
      assignments.audio_primary = value;
    }
    if (const char* value = reinterpret_cast<const char*>(sqlite3_column_text(stmt, 2))) {
      assignments.paging = value;
    }
  }
  sqlite3_finalize(stmt);

  if (assignments.audio_primary && !assignments.audio_primary->empty()) {
    const char* profile_sql =
        "SELECT sample_rate, channels, sample_format FROM audio_operating_profiles "
        "WHERE role = 'primary_audio'";
    if (sqlite3_prepare_v2(db, profile_sql, -1, &stmt, nullptr) == SQLITE_OK &&
        sqlite3_step(stmt) == SQLITE_ROW) {
      homepi::storage::AudioProfileTuple tuple;
      tuple.sample_rate = static_cast<uint32_t>(sqlite3_column_int(stmt, 0));
      tuple.channels = static_cast<uint16_t>(sqlite3_column_int(stmt, 1));
      const char* format = reinterpret_cast<const char*>(sqlite3_column_text(stmt, 2));
      tuple.sample_format =
          homepi::storage::parse_sample_format(format != nullptr ? format : "")
              .value_or(homepi::storage::SampleFormat::S16Le);
      assignments.audio_primary_profile = tuple;
    }
    sqlite3_finalize(stmt);
  }

  return assignments;
}

namespace {

std::optional<std::string> resolve_present_device_id(const std::optional<std::string>& assigned_id,
                                                     const std::vector<UsbDevice>& devices,
                                                     DeviceKind kind) {
  if (!assigned_id || assigned_id->empty()) {
    return std::nullopt;
  }

  const auto exact = std::find_if(
      devices.begin(), devices.end(), [&](const UsbDevice& device) {
        return device.device_id == *assigned_id && device.present;
      });
  if (exact != devices.end()) {
    return assigned_id;
  }

  const auto matches = [&](const UsbDevice& device) {
    return device.kind == kind && device.present &&
           device_identity_matches(device.device_id, *assigned_id);
  };

  const auto resolved = std::find_if(devices.begin(), devices.end(), matches);
  if (resolved == devices.end()) {
    return std::nullopt;
  }
  return resolved->device_id;
}

}  // namespace

bool AssignmentRepository::heal_assignments(UsbAssignments& assignments,
                                            const std::vector<UsbDevice>& devices) {
  bool changed = false;

  const auto heal = [&](std::optional<std::string>& id, DeviceKind kind) {
    if (!id || id->empty()) {
      return;
    }
    const auto resolved = resolve_present_device_id(id, devices, kind);
    if (resolved && *resolved != *id) {
      *id = *resolved;
      changed = true;
    }
  };

  heal(assignments.serial, DeviceKind::Serial);
  heal(assignments.audio_primary, DeviceKind::Audio);
  heal(assignments.paging, DeviceKind::Audio);
  return changed;
}

bool AssignmentRepository::persist_healed_assignments(
    const UsbAssignments& assignments, const std::vector<UsbDevice>& devices) {
  auto* db = static_cast<sqlite3*>(db_);
  const std::string now = utc_now();
  const char* sql =
      "UPDATE usb_assignments SET serial_device_id = ?, audio_primary_device_id = ?, "
      "paging_device_id = ?, updated_at = ? WHERE id = 1";
  sqlite3_stmt* stmt = nullptr;
  if (sqlite3_prepare_v2(db, sql, -1, &stmt, nullptr) != SQLITE_OK) {
    return false;
  }

  const auto bind_optional = [&](int index, const std::optional<std::string>& value) {
    if (!value || value->empty()) {
      sqlite3_bind_null(stmt, index);
      return;
    }
    sqlite3_bind_text(stmt, index, value->c_str(), -1, SQLITE_TRANSIENT);
  };

  bind_optional(1, assignments.serial);
  bind_optional(2, assignments.audio_primary);
  bind_optional(3, assignments.paging);
  sqlite3_bind_text(stmt, 4, now.c_str(), -1, SQLITE_TRANSIENT);

  const bool ok = sqlite3_step(stmt) == SQLITE_DONE;
  sqlite3_finalize(stmt);
  if (!ok) {
    return false;
  }

  if (assignments.audio_primary && !assignments.audio_primary->empty()) {
    const char* profile_sql =
        "UPDATE audio_operating_profiles SET device_id = ? WHERE role IN "
        "('primary_audio', 'platform_loopback')";
    if (sqlite3_prepare_v2(db, profile_sql, -1, &stmt, nullptr) == SQLITE_OK) {
      sqlite3_bind_text(stmt, 1, assignments.audio_primary->c_str(), -1, SQLITE_TRANSIENT);
      sqlite3_step(stmt);
      sqlite3_finalize(stmt);
    }
  }

  sync_hifi_controller_serial(db, assignments, devices);
  return true;
}

bool AssignmentRepository::set_assignments(const UsbAssignments& assignments,
                                             const std::vector<UsbDevice>& devices,
                                             std::string& error_out) {
  const auto ids = {
      assignments.serial, assignments.audio_primary, assignments.paging,
  };
  std::set<std::string> unique;
  for (const auto& id : ids) {
    if (!id || id->empty()) {
      continue;
    }
    if (!unique.insert(*id).second) {
      error_out = "Each role must use a different USB device";
      return false;
    }
  }

  auto validate_role = [&](const std::optional<std::string>& id, DeviceKind expected,
                           const char* label) -> bool {
    if (!id || id->empty()) {
      return true;
    }
    const auto found = std::find_if(devices.begin(), devices.end(),
                                    [&](const UsbDevice& d) { return d.device_id == *id; });
    if (found == devices.end()) {
      error_out = std::string("Unknown device for ") + label;
      return false;
    }
    if (found->kind != expected) {
      error_out = std::string("Invalid device kind for ") + label;
      return false;
    }
    if (!found->present) {
      error_out = std::string("Selected device is not connected for ") + label;
      return false;
    }
    return true;
  };

  if (!validate_role(assignments.serial, DeviceKind::Serial, "serial") ||
      !validate_role(assignments.audio_primary, DeviceKind::Audio, "audioPrimary") ||
      !validate_role(assignments.paging, DeviceKind::Audio, "paging")) {
    return false;
  }

  auto* db = static_cast<sqlite3*>(db_);
  const char* sql =
      "UPDATE usb_assignments SET serial_device_id = ?, audio_primary_device_id = ?, "
      "paging_device_id = ?, updated_at = ? WHERE id = 1";
  sqlite3_stmt* stmt = nullptr;
  sqlite3_prepare_v2(db, sql, -1, &stmt, nullptr);
  const std::string now = utc_now();

  if (assignments.serial && !assignments.serial->empty()) {
    sqlite3_bind_text(stmt, 1, assignments.serial->c_str(), -1, SQLITE_TRANSIENT);
  } else {
    sqlite3_bind_null(stmt, 1);
  }
  if (assignments.audio_primary && !assignments.audio_primary->empty()) {
    sqlite3_bind_text(stmt, 2, assignments.audio_primary->c_str(), -1, SQLITE_TRANSIENT);
  } else {
    sqlite3_bind_null(stmt, 2);
  }
  if (assignments.paging && !assignments.paging->empty()) {
    sqlite3_bind_text(stmt, 3, assignments.paging->c_str(), -1, SQLITE_TRANSIENT);
  } else {
    sqlite3_bind_null(stmt, 3);
  }
  sqlite3_bind_text(stmt, 4, now.c_str(), -1, SQLITE_TRANSIENT);
  const int rc = sqlite3_step(stmt);
  sqlite3_finalize(stmt);
  if (rc != SQLITE_DONE) {
    error_out = "Failed to persist assignments";
    return false;
  }

  sync_hifi_controller_serial(db, assignments, devices);
  return true;
}

bool AssignmentRepository::assignments_degraded(const UsbAssignments& assignments,
                                                const std::vector<UsbDevice>& devices) {
  auto check = [&](const std::optional<std::string>& id, DeviceKind kind) {
    if (!id || id->empty()) {
      return false;
    }
    return !resolve_present_device_id(id, devices, kind).has_value();
  };
  return check(assignments.serial, DeviceKind::Serial) ||
         check(assignments.audio_primary, DeviceKind::Audio) ||
         check(assignments.paging, DeviceKind::Audio);
}

void* AssignmentRepository::db_handle() const { return db_; }

}  // namespace homepi::usb_devices
