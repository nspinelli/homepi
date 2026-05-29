#pragma once

#include <string>
#include <vector>

#include "homepi/hifi-serial/types.hpp"

namespace homepi::hifi_serial {

/**
 * SQLite persistence for Hi-Fi2 state.
 */
class StateRepository {
 public:
  /**
   * Opens the database and runs migrations.
   * @param database_path SQLite path.
   * @param migration_sql Migration SQL text.
   */
  StateRepository(const std::string& database_path, const std::string& migration_sql);

  ~StateRepository();

  StateRepository(const StateRepository&) = delete;
  StateRepository& operator=(const StateRepository&) = delete;

  void apply_parsed_update(const ParsedUpdate& update);

  void set_serial_metadata(const std::string& device_id, const std::string& path);

  void mark_full_sync_complete();

  ControllerState get_controller() const;

  std::vector<ZoneState> get_zones() const;

  std::vector<SourceState> get_sources() const;

  std::vector<GroupState> get_groups() const;

  std::vector<LanguageStringState> get_language_strings() const;

  std::string controller_json() const;

  std::string zones_json() const;

  std::string sources_json() const;

  std::string groups_json() const;

  std::string snapshot_json() const;

 private:
  void* db_ = nullptr;
};

}  // namespace homepi::hifi_serial
