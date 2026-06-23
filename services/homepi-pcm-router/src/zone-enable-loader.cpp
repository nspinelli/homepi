#include "homepi/pcm-router/zone-enable-loader.hpp"

#include <filesystem>

#include <sqlite3.h>

#include "homepi/storage/database-connection.hpp"

namespace homepi::pcm_router {

std::array<bool, kMaxZones + 1> load_enabled_zone_mask(const std::string& database_path,
                                                       int zone_count) {
  std::array<bool, kMaxZones + 1> mask{};
  const int max_zone = zone_count > 0 && zone_count <= kMaxZones ? zone_count : kMaxZones;
  for (int zone_id = 1; zone_id <= max_zone; ++zone_id) {
    mask[zone_id] = true;
  }

  if (!std::filesystem::exists(database_path)) {
    return mask;
  }

  try {
    homepi::storage::DatabaseConnection db(database_path,
                                           homepi::storage::DatabaseOpenMode::ReadOnly);
    sqlite3_stmt* stmt = nullptr;
    if (sqlite3_prepare_v2(db.handle(),
                           "SELECT zone_number, enabled FROM hifi_zones ORDER BY zone_number",
                           -1, &stmt, nullptr) != SQLITE_OK) {
      return mask;
    }

    while (sqlite3_step(stmt) == SQLITE_ROW) {
      const int zone_id = sqlite3_column_int(stmt, 0);
      if (zone_id < 1 || zone_id > max_zone) {
        continue;
      }
      if (sqlite3_column_type(stmt, 1) == SQLITE_NULL) {
        continue;
      }
      mask[zone_id] = sqlite3_column_int(stmt, 1) == 1;
    }
    sqlite3_finalize(stmt);
  } catch (...) {
  }

  return mask;
}

}  // namespace homepi::pcm_router
