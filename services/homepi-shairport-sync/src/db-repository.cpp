#include "homepi/shairport-sync/db-repository.hpp"

#include <chrono>
#include <ctime>
#include <sqlite3.h>
#include <sstream>
#include <stdexcept>

namespace homepi::shairport_sync {

namespace {

std::string utc_now() {
  const auto now = std::chrono::system_clock::now();
  const std::time_t t = std::chrono::system_clock::to_time_t(now);
  std::tm tm{};
  gmtime_r(&t, &tm);
  char buffer[32];
  std::strftime(buffer, sizeof(buffer), "%Y-%m-%dT%H:%M:%SZ", &tm);
  return buffer;
}

bool column_exists(sqlite3* db, const char* table, const char* column) {
  std::string sql = "PRAGMA table_info(" + std::string(table) + ")";
  sqlite3_stmt* stmt = nullptr;
  if (sqlite3_prepare_v2(db, sql.c_str(), -1, &stmt, nullptr) != SQLITE_OK) {
    return false;
  }
  bool found = false;
  while (sqlite3_step(stmt) == SQLITE_ROW) {
    const char* name = reinterpret_cast<const char*>(sqlite3_column_text(stmt, 1));
    if (name != nullptr && std::string(name) == column) {
      found = true;
      break;
    }
  }
  sqlite3_finalize(stmt);
  return found;
}

void apply_migration(sqlite3* db, const std::string& migration_sql) {
  if (!column_exists(db, "hifi_sources", "is_airplay")) {
    char* err = nullptr;
    if (sqlite3_exec(db, migration_sql.c_str(), nullptr, nullptr, &err) != SQLITE_OK) {
      std::string message = err != nullptr ? err : "migration failed";
      sqlite3_free(err);
      throw std::runtime_error(message);
    }
  } else {
    const char* seed_sql =
        "CREATE TABLE IF NOT EXISTS shairport_zone_settings ("
        "zone_number INTEGER PRIMARY KEY CHECK (zone_number BETWEEN 1 AND 16),"
        "volume_control_profile TEXT NOT NULL DEFAULT 'standard',"
        "active_state_timeout REAL NOT NULL DEFAULT 1.0,"
        "session_timeout INTEGER NOT NULL DEFAULT 60,"
        "log_verbosity INTEGER NOT NULL DEFAULT 1,"
        "updated_at TEXT NOT NULL);";
    sqlite3_exec(db, seed_sql, nullptr, nullptr, nullptr);
  }
}

}  // namespace

DbRepository::DbRepository(const std::string& database_path,
                           const std::string& migration_sql) {
  sqlite3* raw = nullptr;
  if (sqlite3_open(database_path.c_str(), &raw) != SQLITE_OK) {
    throw std::runtime_error("Failed to open database: " + database_path);
  }
  db_ = raw;
  apply_migration(static_cast<sqlite3*>(db_), migration_sql);
  seed_zone_settings();
}

DbRepository::~DbRepository() {
  if (db_ != nullptr) {
    sqlite3_close(static_cast<sqlite3*>(db_));
    db_ = nullptr;
  }
}

bool DbRepository::controller_synced() const {
  sqlite3_stmt* stmt = nullptr;
  const char* sql = "SELECT last_full_sync_at FROM hifi_controller WHERE id=1";
  auto* db = static_cast<sqlite3*>(db_);
  if (sqlite3_prepare_v2(db, sql, -1, &stmt, nullptr) != SQLITE_OK) {
    return false;
  }
  bool synced = false;
  if (sqlite3_step(stmt) == SQLITE_ROW &&
      sqlite3_column_type(stmt, 0) != SQLITE_NULL) {
    synced = true;
  }
  sqlite3_finalize(stmt);
  return synced;
}

bool DbRepository::has_zone_data() const {
  sqlite3_stmt* stmt = nullptr;
  const char* sql =
      "SELECT COUNT(*) FROM hifi_zones WHERE name IS NOT NULL AND TRIM(name) != ''";
  auto* db = static_cast<sqlite3*>(db_);
  if (sqlite3_prepare_v2(db, sql, -1, &stmt, nullptr) != SQLITE_OK) {
    return false;
  }
  bool has_data = false;
  if (sqlite3_step(stmt) == SQLITE_ROW) {
    has_data = sqlite3_column_int(stmt, 0) > 0;
  }
  sqlite3_finalize(stmt);
  return has_data;
}

std::optional<int> DbRepository::airplay_source_number() const {
  sqlite3_stmt* stmt = nullptr;
  const char* sql = "SELECT source_number FROM hifi_sources WHERE is_airplay = 1 LIMIT 2";
  auto* db = static_cast<sqlite3*>(db_);
  if (sqlite3_prepare_v2(db, sql, -1, &stmt, nullptr) != SQLITE_OK) {
    return std::nullopt;
  }
  std::optional<int> source;
  int count = 0;
  while (sqlite3_step(stmt) == SQLITE_ROW) {
    source = sqlite3_column_int(stmt, 0);
    ++count;
  }
  sqlite3_finalize(stmt);
  if (count != 1 || !source.has_value()) {
    return std::nullopt;
  }
  return source;
}

void DbRepository::set_airplay_source(int source_number) {
  auto* db = static_cast<sqlite3*>(db_);
  const std::string now = utc_now();
  sqlite3_exec(db, "UPDATE hifi_sources SET is_airplay = 0", nullptr, nullptr, nullptr);
  const std::string sql =
      "INSERT INTO hifi_sources(source_number,is_airplay,updated_at) VALUES(" +
      std::to_string(source_number) + ",1,'" + now +
      "') ON CONFLICT(source_number) DO UPDATE SET is_airplay=1,updated_at=excluded.updated_at";
  sqlite3_exec(db, sql.c_str(), nullptr, nullptr, nullptr);
}

std::vector<ZoneRow> DbRepository::get_zones() const {
  std::vector<ZoneRow> zones;
  sqlite3_stmt* stmt = nullptr;
  const char* sql =
      "SELECT zone_number,name,enabled,initial_volume FROM hifi_zones ORDER BY zone_number";
  auto* db = static_cast<sqlite3*>(db_);
  if (sqlite3_prepare_v2(db, sql, -1, &stmt, nullptr) != SQLITE_OK) {
    return zones;
  }
  while (sqlite3_step(stmt) == SQLITE_ROW) {
    ZoneRow row;
    row.zone_number = sqlite3_column_int(stmt, 0);
    if (sqlite3_column_type(stmt, 1) != SQLITE_NULL) {
      row.name = reinterpret_cast<const char*>(sqlite3_column_text(stmt, 1));
    }
    if (sqlite3_column_type(stmt, 2) != SQLITE_NULL) {
      row.enabled = sqlite3_column_int(stmt, 2);
    }
    if (sqlite3_column_type(stmt, 3) != SQLITE_NULL) {
      row.initial_volume = sqlite3_column_int(stmt, 3);
    }
    zones.push_back(row);
  }
  sqlite3_finalize(stmt);
  return zones;
}

std::vector<ZoneSettings> DbRepository::get_zone_settings() const {
  std::vector<ZoneSettings> settings;
  sqlite3_stmt* stmt = nullptr;
  const char* sql =
      "SELECT zone_number,volume_control_profile,active_state_timeout,session_timeout,"
      "log_verbosity FROM shairport_zone_settings ORDER BY zone_number";
  auto* db = static_cast<sqlite3*>(db_);
  if (sqlite3_prepare_v2(db, sql, -1, &stmt, nullptr) != SQLITE_OK) {
    return settings;
  }
  while (sqlite3_step(stmt) == SQLITE_ROW) {
    ZoneSettings row;
    row.zone_number = sqlite3_column_int(stmt, 0);
    row.volume_control_profile =
        reinterpret_cast<const char*>(sqlite3_column_text(stmt, 1));
    row.active_state_timeout = sqlite3_column_double(stmt, 2);
    row.session_timeout = sqlite3_column_int(stmt, 3);
    row.log_verbosity = sqlite3_column_int(stmt, 4);
    settings.push_back(row);
  }
  sqlite3_finalize(stmt);
  return settings;
}

void DbRepository::seed_zone_settings() {
  auto* db = static_cast<sqlite3*>(db_);
  const std::string now = utc_now();
  for (int zone = 1; zone <= 16; ++zone) {
    const std::string sql =
        "INSERT OR IGNORE INTO shairport_zone_settings(zone_number,updated_at) VALUES(" +
        std::to_string(zone) + ",'" + now + "')";
    sqlite3_exec(db, sql.c_str(), nullptr, nullptr, nullptr);
  }
}

}  // namespace homepi::shairport_sync
