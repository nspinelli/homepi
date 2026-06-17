#include "homepi/metadata/metadata-state-repository.hpp"

#include <filesystem>
#include <fstream>

#include "homepi/events/event-envelope.hpp"
#include "homepi/storage/database-connection.hpp"
#include "homepi/storage/migration-runner.hpp"

#include <sqlite3.h>

namespace fs = std::filesystem;

namespace homepi::metadata {

namespace {

std::string read_migration_sql() {
  return R"SQL(
CREATE TABLE IF NOT EXISTS metadata_owner_state (
  owner_zone_id INTEGER PRIMARY KEY,
  title TEXT NOT NULL DEFAULT '',
  artist TEXT NOT NULL DEFAULT '',
  album TEXT NOT NULL DEFAULT '',
  client_name TEXT NOT NULL DEFAULT '',
  playing INTEGER NOT NULL DEFAULT 0,
  position_ms INTEGER NOT NULL DEFAULT 0,
  duration_ms INTEGER NOT NULL DEFAULT 0,
  has_cover_art INTEGER NOT NULL DEFAULT 0,
  updated_at TEXT NOT NULL DEFAULT ''
);
)SQL";
}

}  // namespace

MetadataStateRepository::MetadataStateRepository(const std::string& database_path)
    : database_path_(database_path) {
  const auto parent = fs::path(database_path_).parent_path();
  if (!parent.empty()) {
    std::error_code ec;
    fs::create_directories(parent, ec);
  }
  db_ = std::make_unique<homepi::storage::DatabaseConnection>(
      database_path_, homepi::storage::DatabaseOpenMode::ReadWrite);
  homepi::storage::MigrationRunner::apply(*db_, read_migration_sql());
}

MetadataStateRepository::~MetadataStateRepository() = default;

void MetadataStateRepository::save_snapshot(const NowPlayingSnapshot& snapshot) {
  if (!db_ || snapshot.owner_zone_id <= 0) {
    return;
  }
  const std::string sql =
      "INSERT INTO metadata_owner_state "
      "(owner_zone_id, title, artist, album, client_name, playing, position_ms, duration_ms, "
      "has_cover_art, updated_at) "
      "VALUES (?, ?, ?, ?, ?, ?, ?, ?, ?, ?) "
      "ON CONFLICT(owner_zone_id) DO UPDATE SET "
      "title=excluded.title, artist=excluded.artist, album=excluded.album, "
      "client_name=excluded.client_name, playing=excluded.playing, "
      "position_ms=excluded.position_ms, duration_ms=excluded.duration_ms, "
      "has_cover_art=excluded.has_cover_art, updated_at=excluded.updated_at;";

  sqlite3_stmt* stmt = nullptr;
  if (sqlite3_prepare_v2(db_->handle(), sql.c_str(), -1, &stmt, nullptr) != SQLITE_OK) {
    return;
  }

  const std::string updated_at = homepi::events::iso_timestamp();
  sqlite3_bind_int(stmt, 1, snapshot.owner_zone_id);
  sqlite3_bind_text(stmt, 2, snapshot.title.c_str(), -1, SQLITE_TRANSIENT);
  sqlite3_bind_text(stmt, 3, snapshot.artist.c_str(), -1, SQLITE_TRANSIENT);
  sqlite3_bind_text(stmt, 4, snapshot.album.c_str(), -1, SQLITE_TRANSIENT);
  sqlite3_bind_text(stmt, 5, snapshot.client_name.c_str(), -1, SQLITE_TRANSIENT);
  sqlite3_bind_int(stmt, 6, snapshot.playing ? 1 : 0);
  sqlite3_bind_int(stmt, 7, snapshot.position_ms);
  sqlite3_bind_int(stmt, 8, snapshot.duration_ms);
  sqlite3_bind_int(stmt, 9, snapshot.has_cover_art ? 1 : 0);
  sqlite3_bind_text(stmt, 10, updated_at.c_str(), -1, SQLITE_TRANSIENT);
  sqlite3_step(stmt);
  sqlite3_finalize(stmt);
}

void MetadataStateRepository::clear_owner(int owner_zone_id) {
  if (!db_ || owner_zone_id <= 0) {
    return;
  }
  sqlite3_stmt* stmt = nullptr;
  const char* sql = "DELETE FROM metadata_owner_state WHERE owner_zone_id = ?;";
  if (sqlite3_prepare_v2(db_->handle(), sql, -1, &stmt, nullptr) != SQLITE_OK) {
    return;
  }
  sqlite3_bind_int(stmt, 1, owner_zone_id);
  sqlite3_step(stmt);
  sqlite3_finalize(stmt);
}

std::optional<NowPlayingSnapshot> MetadataStateRepository::load_owner(int owner_zone_id) const {
  if (!db_ || owner_zone_id <= 0) {
    return std::nullopt;
  }
  sqlite3_stmt* stmt = nullptr;
  const char* sql =
      "SELECT title, artist, album, client_name, playing, position_ms, duration_ms, has_cover_art "
      "FROM metadata_owner_state WHERE owner_zone_id = ? LIMIT 1;";
  if (sqlite3_prepare_v2(db_->handle(), sql, -1, &stmt, nullptr) != SQLITE_OK) {
    return std::nullopt;
  }
  sqlite3_bind_int(stmt, 1, owner_zone_id);
  if (sqlite3_step(stmt) != SQLITE_ROW) {
    sqlite3_finalize(stmt);
    return std::nullopt;
  }

  NowPlayingSnapshot snapshot;
  snapshot.owner_zone_id = owner_zone_id;
  if (const unsigned char* text = sqlite3_column_text(stmt, 0)) {
    snapshot.title = reinterpret_cast<const char*>(text);
  }
  if (const unsigned char* text = sqlite3_column_text(stmt, 1)) {
    snapshot.artist = reinterpret_cast<const char*>(text);
  }
  if (const unsigned char* text = sqlite3_column_text(stmt, 2)) {
    snapshot.album = reinterpret_cast<const char*>(text);
  }
  if (const unsigned char* text = sqlite3_column_text(stmt, 3)) {
    snapshot.client_name = reinterpret_cast<const char*>(text);
  }
  snapshot.playing = sqlite3_column_int(stmt, 4) != 0;
  snapshot.position_ms = sqlite3_column_int(stmt, 5);
  snapshot.duration_ms = sqlite3_column_int(stmt, 6);
  snapshot.has_cover_art = sqlite3_column_int(stmt, 7) != 0;
  sqlite3_finalize(stmt);
  return snapshot;
}

bool MetadataStateRepository::write_cover_art(const std::string& cache_dir, int zone_id,
                                              const std::vector<std::uint8_t>& bytes) {
  if (zone_id <= 0 || bytes.empty()) {
    return false;
  }
  std::error_code ec;
  fs::create_directories(cache_dir, ec);
  const fs::path path = fs::path(cache_dir) / ("cover-zone-" + std::to_string(zone_id));
  std::ofstream out(path, std::ios::binary | std::ios::trunc);
  if (!out) {
    return false;
  }
  out.write(reinterpret_cast<const char*>(bytes.data()),
            static_cast<std::streamsize>(bytes.size()));
  return out.good();
}

}  // namespace homepi::metadata
