#pragma once

#include <optional>
#include <string>
#include <vector>

#include "homepi/shairport-sync/types.hpp"

namespace homepi::shairport_sync {

/**
 * SQLite access for Shairport supervisor state.
 */
class DbRepository {
 public:
  /**
   * Opens the database and applies Shairport migrations.
   * @param database_path SQLite database path.
   * @param migration_sql Migration SQL text.
   */
  DbRepository(const std::string& database_path, const std::string& migration_sql);

  ~DbRepository();

  DbRepository(const DbRepository&) = delete;
  DbRepository& operator=(const DbRepository&) = delete;

  /** @returns True when controller has completed at least one full sync. */
  bool controller_synced() const;

  /** @returns True when zone rows with names exist. */
  bool has_zone_data() const;

  /** @returns Configured AirPlay source number, if exactly one is marked. */
  std::optional<int> airplay_source_number() const;

  /**
   * Sets the AirPlay source designation.
   * @param source_number Source slot 1-8.
   */
  void set_airplay_source(int source_number);

  /** @returns Zone rows from hifi_zones. */
  std::vector<ZoneRow> get_zones() const;

  /** @returns Per-zone Shairport settings rows. */
  std::vector<ZoneSettings> get_zone_settings() const;

  /** Ensures default shairport_zone_settings rows exist. */
  void seed_zone_settings();

 private:
  void* db_ = nullptr;
};

}  // namespace homepi::shairport_sync
