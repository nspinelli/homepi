#include "homepi/audio-orchestrator/airplay-source-loader.hpp"

#include <sqlite3.h>

namespace homepi::audio_orchestrator {

AirplaySourceLoader::AirplaySourceLoader(std::string database_path)
    : database_path_(std::move(database_path)) {}

std::optional<int> AirplaySourceLoader::load_from_database() const {
  sqlite3* db = nullptr;
  if (sqlite3_open_v2(database_path_.c_str(), &db, SQLITE_OPEN_READONLY, nullptr) != SQLITE_OK) {
    if (db != nullptr) {
      sqlite3_close(db);
    }
    return std::nullopt;
  }

  const char* sql = "SELECT source_number FROM hifi_sources WHERE is_airplay = 1 LIMIT 1";
  sqlite3_stmt* stmt = nullptr;
  if (sqlite3_prepare_v2(db, sql, -1, &stmt, nullptr) != SQLITE_OK) {
    sqlite3_close(db);
    return std::nullopt;
  }

  std::optional<int> source;
  if (sqlite3_step(stmt) == SQLITE_ROW) {
    source = sqlite3_column_int(stmt, 0);
  }

  sqlite3_finalize(stmt);
  sqlite3_close(db);
  return source;
}

}  // namespace homepi::audio_orchestrator
