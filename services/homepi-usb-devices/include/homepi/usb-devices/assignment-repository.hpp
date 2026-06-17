#pragma once

#include <optional>
#include <string>
#include <vector>

#include "homepi/usb-devices/types.hpp"

namespace homepi::usb_devices {

/** SQLite persistence for devices and assignments. */
class AssignmentRepository {
 public:
  /**
   * Opens the database and applies migrations.
   * @param database_path SQLite file path.
   * @param migrations_sql SQL migration script contents.
   */
  AssignmentRepository(const std::string& database_path, const std::string& migrations_sql);

  ~AssignmentRepository();

  AssignmentRepository(const AssignmentRepository&) = delete;
  AssignmentRepository& operator=(const AssignmentRepository&) = delete;

  /**
   * Upserts scanned devices and marks missing ones not present.
   * @param scanned Currently connected devices.
   */
  void upsert_devices(const std::vector<UsbDevice>& scanned);

  /**
   * Returns all known devices.
   * @return Device list.
   */
  std::vector<UsbDevice> list_devices() const;

  /**
   * Returns a device by id.
   * @param device_id Device identifier.
   * @return Device if found.
   */
  std::optional<UsbDevice> get_device(const std::string& device_id) const;

  /**
   * Marks a device present or absent.
   * @param device_id Device identifier.
   * @param present Connection state.
   */
  void set_device_present(const std::string& device_id, bool present);

  /**
   * Loads saved role assignments.
   * @return Assignments.
   */
  UsbAssignments get_assignments() const;

  /**
   * Persists role assignments after validation.
   * @param assignments Assignments to save.
   * @param devices Available devices for validation.
   * @param error_out Error message when validation fails.
   * @return True on success.
   */
  bool set_assignments(const UsbAssignments& assignments, const std::vector<UsbDevice>& devices,
                       std::string& error_out);

  /**
   * Returns true when an assigned device is not present.
   * @param assignments Current assignments.
   * @param devices Device list.
   * @return Degraded flag.
   */
  static bool assignments_degraded(const UsbAssignments& assignments,
                                   const std::vector<UsbDevice>& devices);

  /**
   * Returns the underlying sqlite handle for storage writers.
   * @return sqlite3 pointer.
   */
  void* db_handle() const;

 private:
  void* db_ = nullptr;
};

}  // namespace homepi::usb_devices
