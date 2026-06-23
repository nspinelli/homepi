#include "homepi/metadata/metadata-state-repository.hpp"

#include <filesystem>
#include <fstream>
#include <iomanip>
#include <sstream>

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
  track_id TEXT NOT NULL DEFAULT '',
  playing INTEGER NOT NULL DEFAULT 0,
  position_ms INTEGER NOT NULL DEFAULT 0,
  duration_ms INTEGER NOT NULL DEFAULT 0,
  has_cover_art INTEGER NOT NULL DEFAULT 0,
  updated_at TEXT NOT NULL DEFAULT ''
);
CREATE TABLE IF NOT EXISTS track_duration_cache (
  track_id TEXT PRIMARY KEY,
  duration_ms INTEGER NOT NULL,
  updated_at TEXT NOT NULL DEFAULT ''
);
CREATE TABLE IF NOT EXISTS audio_now_playing (
  id INTEGER PRIMARY KEY CHECK (id = 1),
  owner_zone_id INTEGER NOT NULL DEFAULT 0,
  title TEXT NOT NULL DEFAULT '',
  artist TEXT NOT NULL DEFAULT '',
  album TEXT NOT NULL DEFAULT '',
  track_id TEXT NOT NULL DEFAULT '',
  client_name TEXT NOT NULL DEFAULT '',
  playing INTEGER NOT NULL DEFAULT 0,
  position_ms INTEGER NOT NULL DEFAULT 0,
  duration_ms INTEGER NOT NULL DEFAULT 0,
  has_cover_art INTEGER NOT NULL DEFAULT 0,
  cover_art_id TEXT NOT NULL DEFAULT '',
  cover_art_path TEXT NOT NULL DEFAULT '',
  updated_at TEXT NOT NULL DEFAULT ''
);
CREATE TABLE IF NOT EXISTS audio_play_history (
  id INTEGER PRIMARY KEY AUTOINCREMENT,
  stream_key TEXT NOT NULL,
  source TEXT NOT NULL DEFAULT 'airplay',
  zone_id INTEGER NOT NULL,
  title TEXT NOT NULL DEFAULT '',
  artist TEXT NOT NULL DEFAULT '',
  album TEXT NOT NULL DEFAULT '',
  track_id TEXT NOT NULL DEFAULT '',
  client_name TEXT NOT NULL DEFAULT '',
  duration_ms INTEGER NOT NULL DEFAULT 0,
  played_at TEXT NOT NULL DEFAULT ''
);
)SQL";
}

std::string sha256_hex(const std::vector<std::uint8_t>& bytes) {
  // FNV-1a 64-bit fingerprint used as a stable content id when OpenSSL is unavailable.
  std::uint64_t hash = 14695981039346656037ULL;
  for (const std::uint8_t byte : bytes) {
    hash ^= byte;
    hash *= 1099511628211ULL;
  }
  std::ostringstream out;
  out << std::hex << std::setfill('0') << std::setw(16) << hash;
  return out.str();
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
  char* error_message = nullptr;
  if (sqlite3_exec(db_->handle(),
                   "ALTER TABLE metadata_owner_state ADD COLUMN track_id TEXT NOT NULL DEFAULT '';",
                   nullptr, nullptr, &error_message) != SQLITE_OK) {
    sqlite3_free(error_message);
  }
}

MetadataStateRepository::~MetadataStateRepository() = default;

std::string MetadataStateRepository::artwork_directory(const std::string& cache_dir) {
  return (fs::path(cache_dir) / "metadata" / "artwork").string();
}

void MetadataStateRepository::save_snapshot(const NowPlayingSnapshot& snapshot) {
  if (!db_ || snapshot.owner_zone_id <= 0) {
    return;
  }
  const std::string updated_at = homepi::events::iso_timestamp();
  const std::string sql =
      "INSERT INTO metadata_owner_state "
      "(owner_zone_id, title, artist, album, client_name, track_id, playing, position_ms, duration_ms, "
      "has_cover_art, updated_at) "
      "VALUES (?, ?, ?, ?, ?, ?, ?, ?, ?, ?, ?) "
      "ON CONFLICT(owner_zone_id) DO UPDATE SET "
      "title=excluded.title, artist=excluded.artist, album=excluded.album, "
      "client_name=excluded.client_name, track_id=excluded.track_id, playing=excluded.playing, "
      "position_ms=excluded.position_ms, duration_ms=excluded.duration_ms, "
      "has_cover_art=excluded.has_cover_art, updated_at=excluded.updated_at;";

  sqlite3_stmt* stmt = nullptr;
  if (sqlite3_prepare_v2(db_->handle(), sql.c_str(), -1, &stmt, nullptr) != SQLITE_OK) {
    return;
  }

  sqlite3_bind_int(stmt, 1, snapshot.owner_zone_id);
  sqlite3_bind_text(stmt, 2, snapshot.title.c_str(), -1, SQLITE_TRANSIENT);
  sqlite3_bind_text(stmt, 3, snapshot.artist.c_str(), -1, SQLITE_TRANSIENT);
  sqlite3_bind_text(stmt, 4, snapshot.album.c_str(), -1, SQLITE_TRANSIENT);
  sqlite3_bind_text(stmt, 5, snapshot.client_name.c_str(), -1, SQLITE_TRANSIENT);
  sqlite3_bind_text(stmt, 6, snapshot.track_id.c_str(), -1, SQLITE_TRANSIENT);
  sqlite3_bind_int(stmt, 7, snapshot.playing ? 1 : 0);
  sqlite3_bind_int(stmt, 8, snapshot.position_ms);
  sqlite3_bind_int(stmt, 9, snapshot.duration_ms);
  sqlite3_bind_int(stmt, 10, snapshot.has_cover_art ? 1 : 0);
  sqlite3_bind_text(stmt, 11, updated_at.c_str(), -1, SQLITE_TRANSIENT);
  sqlite3_step(stmt);
  sqlite3_finalize(stmt);

  const std::string cover_path =
      snapshot.has_cover_art ? artwork_directory("") : "";
  (void)cover_path;

  const char* now_playing_sql =
      "INSERT INTO audio_now_playing "
      "(id, owner_zone_id, title, artist, album, track_id, client_name, playing, position_ms, "
      "duration_ms, has_cover_art, updated_at) "
      "VALUES (1, ?, ?, ?, ?, ?, ?, ?, ?, ?, ?, ?) "
      "ON CONFLICT(id) DO UPDATE SET "
      "owner_zone_id=excluded.owner_zone_id, title=excluded.title, artist=excluded.artist, "
      "album=excluded.album, track_id=excluded.track_id, client_name=excluded.client_name, "
      "playing=excluded.playing, position_ms=excluded.position_ms, duration_ms=excluded.duration_ms, "
      "has_cover_art=excluded.has_cover_art, updated_at=excluded.updated_at;";
  if (sqlite3_prepare_v2(db_->handle(), now_playing_sql, -1, &stmt, nullptr) != SQLITE_OK) {
    return;
  }
  sqlite3_bind_int(stmt, 1, snapshot.owner_zone_id);
  sqlite3_bind_text(stmt, 2, snapshot.title.c_str(), -1, SQLITE_TRANSIENT);
  sqlite3_bind_text(stmt, 3, snapshot.artist.c_str(), -1, SQLITE_TRANSIENT);
  sqlite3_bind_text(stmt, 4, snapshot.album.c_str(), -1, SQLITE_TRANSIENT);
  sqlite3_bind_text(stmt, 5, snapshot.track_id.c_str(), -1, SQLITE_TRANSIENT);
  sqlite3_bind_text(stmt, 6, snapshot.client_name.c_str(), -1, SQLITE_TRANSIENT);
  sqlite3_bind_int(stmt, 7, snapshot.playing ? 1 : 0);
  sqlite3_bind_int(stmt, 8, snapshot.position_ms);
  sqlite3_bind_int(stmt, 9, snapshot.duration_ms);
  sqlite3_bind_int(stmt, 10, snapshot.has_cover_art ? 1 : 0);
  sqlite3_bind_text(stmt, 11, updated_at.c_str(), -1, SQLITE_TRANSIENT);
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

  const char* clear_now_playing =
      "UPDATE audio_now_playing SET owner_zone_id=0, title='', artist='', album='', track_id='', "
      "client_name='', playing=0, position_ms=0, duration_ms=0, has_cover_art=0, "
      "cover_art_id='', cover_art_path='', updated_at=? WHERE id=1;";
  if (sqlite3_prepare_v2(db_->handle(), clear_now_playing, -1, &stmt, nullptr) != SQLITE_OK) {
    return;
  }
  const std::string updated_at = homepi::events::iso_timestamp();
  sqlite3_bind_text(stmt, 1, updated_at.c_str(), -1, SQLITE_TRANSIENT);
  sqlite3_step(stmt);
  sqlite3_finalize(stmt);
}

std::optional<NowPlayingSnapshot> MetadataStateRepository::load_owner(int owner_zone_id) const {
  if (!db_ || owner_zone_id <= 0) {
    return std::nullopt;
  }
  sqlite3_stmt* stmt = nullptr;
  const char* sql =
      "SELECT title, artist, album, client_name, track_id, playing, position_ms, duration_ms, has_cover_art "
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
  if (const unsigned char* text = sqlite3_column_text(stmt, 4)) {
    snapshot.track_id = reinterpret_cast<const char*>(text);
  }
  snapshot.playing = sqlite3_column_int(stmt, 5) != 0;
  snapshot.position_ms = sqlite3_column_int(stmt, 6);
  snapshot.duration_ms = sqlite3_column_int(stmt, 7);
  snapshot.has_cover_art = sqlite3_column_int(stmt, 8) != 0;
  sqlite3_finalize(stmt);
  return snapshot;
}

std::string MetadataStateRepository::write_cover_art(const std::string& cache_dir, int zone_id,
                                                     const std::vector<std::uint8_t>& bytes) {
  if (zone_id <= 0 || bytes.empty()) {
    return {};
  }

  const std::string art_dir = artwork_directory(cache_dir);
  std::error_code ec;
  fs::create_directories(art_dir, ec);

  const std::string art_id = sha256_hex(bytes);
  const fs::path hashed = fs::path(art_dir) / ("sha256-" + art_id + ".jpg");
  const fs::path current = fs::path(art_dir) / "current.jpg";
  const fs::path legacy = fs::path(cache_dir) / ("cover-zone-" + std::to_string(zone_id));

  if (!fs::exists(hashed, ec)) {
    std::ofstream hashed_out(hashed, std::ios::binary | std::ios::trunc);
    if (!hashed_out) {
      return {};
    }
    hashed_out.write(reinterpret_cast<const char*>(bytes.data()),
                     static_cast<std::streamsize>(bytes.size()));
    if (!hashed_out.good()) {
      return {};
    }
  }

  std::ofstream current_out(current, std::ios::binary | std::ios::trunc);
  if (!current_out) {
    return {};
  }
  current_out.write(reinterpret_cast<const char*>(bytes.data()),
                    static_cast<std::streamsize>(bytes.size()));
  if (!current_out.good()) {
    return {};
  }

  std::ofstream legacy_out(legacy, std::ios::binary | std::ios::trunc);
  if (legacy_out) {
    legacy_out.write(reinterpret_cast<const char*>(bytes.data()),
                     static_cast<std::streamsize>(bytes.size()));
  }

  return art_id;
}

bool MetadataStateRepository::delete_cover_art(const std::string& cache_dir, int zone_id) {
  if (zone_id <= 0) {
    return false;
  }
  std::error_code ec;
  fs::remove(fs::path(cache_dir) / ("cover-zone-" + std::to_string(zone_id)), ec);
  fs::remove(fs::path(artwork_directory(cache_dir)) / "current.jpg", ec);
  return true;
}

void MetadataStateRepository::cache_track_duration(const std::string& track_id, int duration_ms) {
  if (!db_ || track_id.empty() || duration_ms <= 0) {
    return;
  }
  sqlite3_stmt* stmt = nullptr;
  const char* sql =
      "INSERT INTO track_duration_cache (track_id, duration_ms, updated_at) "
      "VALUES (?, ?, ?) "
      "ON CONFLICT(track_id) DO UPDATE SET "
      "duration_ms=excluded.duration_ms, updated_at=excluded.updated_at;";
  if (sqlite3_prepare_v2(db_->handle(), sql, -1, &stmt, nullptr) != SQLITE_OK) {
    return;
  }
  const std::string updated_at = homepi::events::iso_timestamp();
  sqlite3_bind_text(stmt, 1, track_id.c_str(), -1, SQLITE_TRANSIENT);
  sqlite3_bind_int(stmt, 2, duration_ms);
  sqlite3_bind_text(stmt, 3, updated_at.c_str(), -1, SQLITE_TRANSIENT);
  sqlite3_step(stmt);
  sqlite3_finalize(stmt);
}

std::optional<int> MetadataStateRepository::load_cached_track_duration(
    const std::string& track_id) const {
  if (!db_ || track_id.empty()) {
    return std::nullopt;
  }
  sqlite3_stmt* stmt = nullptr;
  const char* sql =
      "SELECT duration_ms FROM track_duration_cache WHERE track_id = ? LIMIT 1;";
  if (sqlite3_prepare_v2(db_->handle(), sql, -1, &stmt, nullptr) != SQLITE_OK) {
    return std::nullopt;
  }
  sqlite3_bind_text(stmt, 1, track_id.c_str(), -1, SQLITE_TRANSIENT);
  if (sqlite3_step(stmt) != SQLITE_ROW) {
    sqlite3_finalize(stmt);
    return std::nullopt;
  }
  const int duration_ms = sqlite3_column_int(stmt, 0);
  sqlite3_finalize(stmt);
  if (duration_ms <= 0) {
    return std::nullopt;
  }
  return duration_ms;
}

void MetadataStateRepository::record_play_history(const NowPlayingSnapshot& snapshot) {
  if (!db_ || snapshot.owner_zone_id <= 0) {
    return;
  }
  if (snapshot.title.empty() && snapshot.artist.empty()) {
    return;
  }

  const std::string stream_key =
      snapshot.track_id.empty()
          ? snapshot.title + "|" + snapshot.artist + "|" + std::to_string(snapshot.owner_zone_id)
          : snapshot.track_id;
  const std::string played_at = homepi::events::iso_timestamp();

  sqlite3_stmt* stmt = nullptr;
  const char* insert_sql =
      "INSERT INTO audio_play_history "
      "(stream_key, source, zone_id, title, artist, album, track_id, client_name, duration_ms, "
      "played_at) "
      "VALUES (?, 'airplay', ?, ?, ?, ?, ?, ?, ?, ?);";
  if (sqlite3_prepare_v2(db_->handle(), insert_sql, -1, &stmt, nullptr) != SQLITE_OK) {
    return;
  }
  sqlite3_bind_text(stmt, 1, stream_key.c_str(), -1, SQLITE_TRANSIENT);
  sqlite3_bind_int(stmt, 2, snapshot.owner_zone_id);
  sqlite3_bind_text(stmt, 3, snapshot.title.c_str(), -1, SQLITE_TRANSIENT);
  sqlite3_bind_text(stmt, 4, snapshot.artist.c_str(), -1, SQLITE_TRANSIENT);
  sqlite3_bind_text(stmt, 5, snapshot.album.c_str(), -1, SQLITE_TRANSIENT);
  sqlite3_bind_text(stmt, 6, snapshot.track_id.c_str(), -1, SQLITE_TRANSIENT);
  sqlite3_bind_text(stmt, 7, snapshot.client_name.c_str(), -1, SQLITE_TRANSIENT);
  sqlite3_bind_int(stmt, 8, snapshot.duration_ms > 0 ? snapshot.duration_ms : snapshot.position_ms);
  sqlite3_bind_text(stmt, 9, played_at.c_str(), -1, SQLITE_TRANSIENT);
  sqlite3_step(stmt);
  sqlite3_finalize(stmt);

  sqlite3_exec(db_->handle(),
               "DELETE FROM audio_play_history WHERE id NOT IN "
               "(SELECT id FROM audio_play_history ORDER BY id DESC LIMIT 20);",
               nullptr, nullptr, nullptr);
}

}  // namespace homepi::metadata
