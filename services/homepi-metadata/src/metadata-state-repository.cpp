#include "homepi/metadata/metadata-state-repository.hpp"

#include <filesystem>
#include <fstream>
#include <unordered_set>

#include "homepi/events/event-envelope.hpp"
#include "homepi/metadata/content-hash.hpp"
#include "homepi/metadata/metadata-normalizer.hpp"
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
  genre TEXT NOT NULL DEFAULT '',
  composer TEXT NOT NULL DEFAULT '',
  comment TEXT NOT NULL DEFAULT '',
  sort_title TEXT NOT NULL DEFAULT '',
  file_kind TEXT NOT NULL DEFAULT '',
  stream_url TEXT NOT NULL DEFAULT '',
  client_name TEXT NOT NULL DEFAULT '',
  client_model TEXT NOT NULL DEFAULT '',
  client_user_agent TEXT NOT NULL DEFAULT '',
  client_ip TEXT NOT NULL DEFAULT '',
  client_device_id TEXT NOT NULL DEFAULT '',
  client_mac TEXT NOT NULL DEFAULT '',
  track_id TEXT NOT NULL DEFAULT '',
  persistent_id TEXT NOT NULL DEFAULT '',
  playing INTEGER NOT NULL DEFAULT 0,
  position_ms INTEGER NOT NULL DEFAULT 0,
  duration_ms INTEGER NOT NULL DEFAULT 0,
  has_cover_art INTEGER NOT NULL DEFAULT 0,
  cover_art_id TEXT NOT NULL DEFAULT '',
  cover_art_path TEXT NOT NULL DEFAULT '',
  metadata_quality TEXT NOT NULL DEFAULT 'empty',
  started_at TEXT NOT NULL DEFAULT '',
  updated_at TEXT NOT NULL DEFAULT ''
);
CREATE TABLE IF NOT EXISTS track_duration_cache (
  track_key TEXT PRIMARY KEY,
  duration_ms INTEGER NOT NULL,
  updated_at TEXT NOT NULL DEFAULT ''
);
CREATE TABLE IF NOT EXISTS audio_now_playing (
  id INTEGER PRIMARY KEY CHECK (id = 1),
  owner_zone_id INTEGER NOT NULL DEFAULT 0,
  title TEXT NOT NULL DEFAULT '',
  artist TEXT NOT NULL DEFAULT '',
  album TEXT NOT NULL DEFAULT '',
  genre TEXT NOT NULL DEFAULT '',
  composer TEXT NOT NULL DEFAULT '',
  comment TEXT NOT NULL DEFAULT '',
  sort_title TEXT NOT NULL DEFAULT '',
  file_kind TEXT NOT NULL DEFAULT '',
  stream_url TEXT NOT NULL DEFAULT '',
  track_id TEXT NOT NULL DEFAULT '',
  persistent_id TEXT NOT NULL DEFAULT '',
  client_name TEXT NOT NULL DEFAULT '',
  client_model TEXT NOT NULL DEFAULT '',
  client_user_agent TEXT NOT NULL DEFAULT '',
  client_ip TEXT NOT NULL DEFAULT '',
  client_device_id TEXT NOT NULL DEFAULT '',
  client_mac TEXT NOT NULL DEFAULT '',
  playing INTEGER NOT NULL DEFAULT 0,
  position_ms INTEGER NOT NULL DEFAULT 0,
  duration_ms INTEGER NOT NULL DEFAULT 0,
  has_cover_art INTEGER NOT NULL DEFAULT 0,
  cover_art_id TEXT NOT NULL DEFAULT '',
  cover_art_path TEXT NOT NULL DEFAULT '',
  metadata_quality TEXT NOT NULL DEFAULT 'empty',
  started_at TEXT NOT NULL DEFAULT '',
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
  persistent_id TEXT NOT NULL DEFAULT '',
  client_name TEXT NOT NULL DEFAULT '',
  client_model TEXT NOT NULL DEFAULT '',
  client_ip TEXT NOT NULL DEFAULT '',
  duration_ms INTEGER NOT NULL DEFAULT 0,
  last_position_ms INTEGER NOT NULL DEFAULT 0,
  has_cover_art INTEGER NOT NULL DEFAULT 0,
  cover_art_id TEXT NOT NULL DEFAULT '',
  cover_art_path TEXT NOT NULL DEFAULT '',
  started_at TEXT NOT NULL DEFAULT '',
  ended_at TEXT NOT NULL DEFAULT '',
  created_at TEXT NOT NULL DEFAULT '',
  played_at TEXT NOT NULL DEFAULT ''
);
)SQL";
}

void try_add_column(sqlite3* db, const char* sql) {
  char* error_message = nullptr;
  if (sqlite3_exec(db, sql, nullptr, nullptr, &error_message) != SQLITE_OK) {
    sqlite3_free(error_message);
  }
}

}  // namespace

MetadataStateRepository::MetadataStateRepository(const std::string& database_path,
                                                 const std::string& cache_dir)
    : database_path_(database_path), cache_dir_(cache_dir) {
  const auto parent = fs::path(database_path_).parent_path();
  if (!parent.empty()) {
    std::error_code ec;
    fs::create_directories(parent, ec);
  }
  db_ = std::make_unique<homepi::storage::DatabaseConnection>(
      database_path_, homepi::storage::DatabaseOpenMode::ReadWrite);
  homepi::storage::MigrationRunner::apply(*db_, read_migration_sql());
  apply_column_migrations();
}

MetadataStateRepository::~MetadataStateRepository() = default;

void MetadataStateRepository::apply_column_migrations() {
  if (!db_) {
    return;
  }
  sqlite3* db = db_->handle();
  try_add_column(db, "ALTER TABLE metadata_owner_state ADD COLUMN track_id TEXT NOT NULL DEFAULT '';");
  try_add_column(db, "ALTER TABLE metadata_owner_state ADD COLUMN genre TEXT NOT NULL DEFAULT '';");
  try_add_column(db, "ALTER TABLE metadata_owner_state ADD COLUMN composer TEXT NOT NULL DEFAULT '';");
  try_add_column(db, "ALTER TABLE metadata_owner_state ADD COLUMN comment TEXT NOT NULL DEFAULT '';");
  try_add_column(db, "ALTER TABLE metadata_owner_state ADD COLUMN sort_title TEXT NOT NULL DEFAULT '';");
  try_add_column(db, "ALTER TABLE metadata_owner_state ADD COLUMN file_kind TEXT NOT NULL DEFAULT '';");
  try_add_column(db, "ALTER TABLE metadata_owner_state ADD COLUMN stream_url TEXT NOT NULL DEFAULT '';");
  try_add_column(db, "ALTER TABLE metadata_owner_state ADD COLUMN persistent_id TEXT NOT NULL DEFAULT '';");
  try_add_column(db, "ALTER TABLE metadata_owner_state ADD COLUMN client_model TEXT NOT NULL DEFAULT '';");
  try_add_column(db, "ALTER TABLE metadata_owner_state ADD COLUMN client_user_agent TEXT NOT NULL DEFAULT '';");
  try_add_column(db, "ALTER TABLE metadata_owner_state ADD COLUMN client_ip TEXT NOT NULL DEFAULT '';");
  try_add_column(db, "ALTER TABLE metadata_owner_state ADD COLUMN client_device_id TEXT NOT NULL DEFAULT '';");
  try_add_column(db, "ALTER TABLE metadata_owner_state ADD COLUMN client_mac TEXT NOT NULL DEFAULT '';");
  try_add_column(db, "ALTER TABLE metadata_owner_state ADD COLUMN cover_art_id TEXT NOT NULL DEFAULT '';");
  try_add_column(db, "ALTER TABLE metadata_owner_state ADD COLUMN cover_art_path TEXT NOT NULL DEFAULT '';");
  try_add_column(db, "ALTER TABLE metadata_owner_state ADD COLUMN metadata_quality TEXT NOT NULL DEFAULT 'empty';");
  try_add_column(db, "ALTER TABLE metadata_owner_state ADD COLUMN started_at TEXT NOT NULL DEFAULT '';");
  try_add_column(db, "ALTER TABLE audio_now_playing ADD COLUMN genre TEXT NOT NULL DEFAULT '';");
  try_add_column(db, "ALTER TABLE audio_now_playing ADD COLUMN composer TEXT NOT NULL DEFAULT '';");
  try_add_column(db, "ALTER TABLE audio_now_playing ADD COLUMN comment TEXT NOT NULL DEFAULT '';");
  try_add_column(db, "ALTER TABLE audio_now_playing ADD COLUMN sort_title TEXT NOT NULL DEFAULT '';");
  try_add_column(db, "ALTER TABLE audio_now_playing ADD COLUMN file_kind TEXT NOT NULL DEFAULT '';");
  try_add_column(db, "ALTER TABLE audio_now_playing ADD COLUMN stream_url TEXT NOT NULL DEFAULT '';");
  try_add_column(db, "ALTER TABLE audio_now_playing ADD COLUMN persistent_id TEXT NOT NULL DEFAULT '';");
  try_add_column(db, "ALTER TABLE audio_now_playing ADD COLUMN client_model TEXT NOT NULL DEFAULT '';");
  try_add_column(db, "ALTER TABLE audio_now_playing ADD COLUMN client_user_agent TEXT NOT NULL DEFAULT '';");
  try_add_column(db, "ALTER TABLE audio_now_playing ADD COLUMN client_ip TEXT NOT NULL DEFAULT '';");
  try_add_column(db, "ALTER TABLE audio_now_playing ADD COLUMN client_device_id TEXT NOT NULL DEFAULT '';");
  try_add_column(db, "ALTER TABLE audio_now_playing ADD COLUMN client_mac TEXT NOT NULL DEFAULT '';");
  try_add_column(db, "ALTER TABLE audio_now_playing ADD COLUMN metadata_quality TEXT NOT NULL DEFAULT 'empty';");
  try_add_column(db, "ALTER TABLE audio_now_playing ADD COLUMN started_at TEXT NOT NULL DEFAULT '';");
  try_add_column(db, "ALTER TABLE audio_play_history ADD COLUMN persistent_id TEXT NOT NULL DEFAULT '';");
  try_add_column(db, "ALTER TABLE audio_play_history ADD COLUMN client_model TEXT NOT NULL DEFAULT '';");
  try_add_column(db, "ALTER TABLE audio_play_history ADD COLUMN client_ip TEXT NOT NULL DEFAULT '';");
  try_add_column(db, "ALTER TABLE audio_play_history ADD COLUMN last_position_ms INTEGER NOT NULL DEFAULT 0;");
  try_add_column(db, "ALTER TABLE audio_play_history ADD COLUMN has_cover_art INTEGER NOT NULL DEFAULT 0;");
  try_add_column(db, "ALTER TABLE audio_play_history ADD COLUMN cover_art_id TEXT NOT NULL DEFAULT '';");
  try_add_column(db, "ALTER TABLE audio_play_history ADD COLUMN cover_art_path TEXT NOT NULL DEFAULT '';");
  try_add_column(db, "ALTER TABLE audio_play_history ADD COLUMN started_at TEXT NOT NULL DEFAULT '';");
  try_add_column(db, "ALTER TABLE audio_play_history ADD COLUMN ended_at TEXT NOT NULL DEFAULT '';");
  try_add_column(db, "ALTER TABLE audio_play_history ADD COLUMN created_at TEXT NOT NULL DEFAULT '';");
  try_add_column(db, "ALTER TABLE audio_play_history ADD COLUMN played_at TEXT NOT NULL DEFAULT '';");
  sqlite3_exec(db,
               "ALTER TABLE track_duration_cache RENAME COLUMN track_id TO track_key;",
               nullptr, nullptr, nullptr);
  sqlite3_exec(db,
               "CREATE INDEX IF NOT EXISTS idx_audio_play_history_ended_at "
               "ON audio_play_history(ended_at DESC);",
               nullptr, nullptr, nullptr);
}

std::string MetadataStateRepository::artwork_directory(const std::string& cache_dir) {
  return (fs::path(cache_dir) / "metadata" / "artwork").string();
}

std::string MetadataStateRepository::track_key_for(const NowPlayingSnapshot& snapshot) {
  if (is_persistent_id_like(snapshot.track_id)) {
    return snapshot.track_id;
  }
  if (is_persistent_id_like(snapshot.persistent_id)) {
    return snapshot.persistent_id;
  }
  return {};
}

bool MetadataStateRepository::is_meaningful_stream(const NowPlayingSnapshot& snapshot) {
  return !snapshot.title.empty() || !snapshot.artist.empty() || !snapshot.album.empty() ||
         !snapshot.track_id.empty() || !snapshot.persistent_id.empty() || snapshot.has_cover_art ||
         snapshot.position_ms >= 10000;
}

void MetadataStateRepository::save_snapshot(const NowPlayingSnapshot& snapshot) {
  std::lock_guard lock(db_mutex_);
  if (!db_ || snapshot.owner_zone_id <= 0) {
    return;
  }
  const std::string updated_at = homepi::events::iso_timestamp();
  const std::string cover_path = snapshot.cover_art_path.empty() && snapshot.has_cover_art
                                     ? artwork_directory(cache_dir_) + "/sha256-" +
                                           snapshot.cover_art_id + ".jpg"
                                     : snapshot.cover_art_path;

  const char* owner_sql =
      "INSERT INTO metadata_owner_state "
      "(owner_zone_id, title, artist, album, genre, composer, comment, sort_title, file_kind, "
      "stream_url, client_name, client_model, client_user_agent, client_ip, client_device_id, "
      "client_mac, track_id, persistent_id, playing, position_ms, duration_ms, has_cover_art, "
      "cover_art_id, cover_art_path, metadata_quality, started_at, updated_at) "
      "VALUES (?, ?, ?, ?, ?, ?, ?, ?, ?, ?, ?, ?, ?, ?, ?, ?, ?, ?, ?, ?, ?, ?, ?, ?, ?, ?, ?) "
      "ON CONFLICT(owner_zone_id) DO UPDATE SET "
      "title=excluded.title, artist=excluded.artist, album=excluded.album, genre=excluded.genre, "
      "composer=excluded.composer, comment=excluded.comment, sort_title=excluded.sort_title, "
      "file_kind=excluded.file_kind, stream_url=excluded.stream_url, "
      "client_name=excluded.client_name, client_model=excluded.client_model, "
      "client_user_agent=excluded.client_user_agent, client_ip=excluded.client_ip, "
      "client_device_id=excluded.client_device_id, client_mac=excluded.client_mac, "
      "track_id=excluded.track_id, persistent_id=excluded.persistent_id, playing=excluded.playing, "
      "position_ms=excluded.position_ms, duration_ms=excluded.duration_ms, "
      "has_cover_art=excluded.has_cover_art, cover_art_id=excluded.cover_art_id, "
      "cover_art_path=excluded.cover_art_path, metadata_quality=excluded.metadata_quality, "
      "started_at=excluded.started_at, updated_at=excluded.updated_at;";

  sqlite3_stmt* stmt = nullptr;
  if (sqlite3_prepare_v2(db_->handle(), owner_sql, -1, &stmt, nullptr) != SQLITE_OK) {
    return;
  }

  sqlite3_bind_int(stmt, 1, snapshot.owner_zone_id);
  sqlite3_bind_text(stmt, 2, snapshot.title.c_str(), -1, SQLITE_TRANSIENT);
  sqlite3_bind_text(stmt, 3, snapshot.artist.c_str(), -1, SQLITE_TRANSIENT);
  sqlite3_bind_text(stmt, 4, snapshot.album.c_str(), -1, SQLITE_TRANSIENT);
  sqlite3_bind_text(stmt, 5, snapshot.genre.c_str(), -1, SQLITE_TRANSIENT);
  sqlite3_bind_text(stmt, 6, snapshot.composer.c_str(), -1, SQLITE_TRANSIENT);
  sqlite3_bind_text(stmt, 7, snapshot.comment.c_str(), -1, SQLITE_TRANSIENT);
  sqlite3_bind_text(stmt, 8, snapshot.sort_title.c_str(), -1, SQLITE_TRANSIENT);
  sqlite3_bind_text(stmt, 9, snapshot.file_kind.c_str(), -1, SQLITE_TRANSIENT);
  sqlite3_bind_text(stmt, 10, snapshot.stream_url.c_str(), -1, SQLITE_TRANSIENT);
  sqlite3_bind_text(stmt, 11, snapshot.client_name.c_str(), -1, SQLITE_TRANSIENT);
  sqlite3_bind_text(stmt, 12, snapshot.client_model.c_str(), -1, SQLITE_TRANSIENT);
  sqlite3_bind_text(stmt, 13, snapshot.client_user_agent.c_str(), -1, SQLITE_TRANSIENT);
  sqlite3_bind_text(stmt, 14, snapshot.client_ip.c_str(), -1, SQLITE_TRANSIENT);
  sqlite3_bind_text(stmt, 15, snapshot.client_device_id.c_str(), -1, SQLITE_TRANSIENT);
  sqlite3_bind_text(stmt, 16, snapshot.client_mac.c_str(), -1, SQLITE_TRANSIENT);
  sqlite3_bind_text(stmt, 17, snapshot.track_id.c_str(), -1, SQLITE_TRANSIENT);
  sqlite3_bind_text(stmt, 18, snapshot.persistent_id.c_str(), -1, SQLITE_TRANSIENT);
  sqlite3_bind_int(stmt, 19, snapshot.playing ? 1 : 0);
  sqlite3_bind_int(stmt, 20, snapshot.position_ms);
  sqlite3_bind_int(stmt, 21, snapshot.duration_ms);
  sqlite3_bind_int(stmt, 22, snapshot.has_cover_art ? 1 : 0);
  sqlite3_bind_text(stmt, 23, snapshot.cover_art_id.c_str(), -1, SQLITE_TRANSIENT);
  sqlite3_bind_text(stmt, 24, cover_path.c_str(), -1, SQLITE_TRANSIENT);
  sqlite3_bind_text(stmt, 25, snapshot.metadata_quality.c_str(), -1, SQLITE_TRANSIENT);
  sqlite3_bind_text(stmt, 26, snapshot.started_at.c_str(), -1, SQLITE_TRANSIENT);
  sqlite3_bind_text(stmt, 27, updated_at.c_str(), -1, SQLITE_TRANSIENT);
  sqlite3_step(stmt);
  sqlite3_finalize(stmt);

  const char* now_playing_sql =
      "INSERT INTO audio_now_playing "
      "(id, owner_zone_id, title, artist, album, genre, composer, comment, sort_title, file_kind, "
      "stream_url, track_id, persistent_id, client_name, client_model, client_user_agent, "
      "client_ip, client_device_id, client_mac, playing, position_ms, duration_ms, has_cover_art, "
      "cover_art_id, cover_art_path, metadata_quality, started_at, updated_at) "
      "VALUES (1, ?, ?, ?, ?, ?, ?, ?, ?, ?, ?, ?, ?, ?, ?, ?, ?, ?, ?, ?, ?, ?, ?, ?, ?, ?, ?, ?) "
      "ON CONFLICT(id) DO UPDATE SET "
      "owner_zone_id=excluded.owner_zone_id, title=excluded.title, artist=excluded.artist, "
      "album=excluded.album, genre=excluded.genre, composer=excluded.composer, "
      "comment=excluded.comment, sort_title=excluded.sort_title, file_kind=excluded.file_kind, "
      "stream_url=excluded.stream_url, track_id=excluded.track_id, "
      "persistent_id=excluded.persistent_id, client_name=excluded.client_name, "
      "client_model=excluded.client_model, client_user_agent=excluded.client_user_agent, "
      "client_ip=excluded.client_ip, client_device_id=excluded.client_device_id, "
      "client_mac=excluded.client_mac, playing=excluded.playing, "
      "position_ms=excluded.position_ms, duration_ms=excluded.duration_ms, "
      "has_cover_art=excluded.has_cover_art, cover_art_id=excluded.cover_art_id, "
      "cover_art_path=excluded.cover_art_path, metadata_quality=excluded.metadata_quality, "
      "started_at=excluded.started_at, updated_at=excluded.updated_at;";
  if (sqlite3_prepare_v2(db_->handle(), now_playing_sql, -1, &stmt, nullptr) != SQLITE_OK) {
    return;
  }
  sqlite3_bind_int(stmt, 1, snapshot.owner_zone_id);
  sqlite3_bind_text(stmt, 2, snapshot.title.c_str(), -1, SQLITE_TRANSIENT);
  sqlite3_bind_text(stmt, 3, snapshot.artist.c_str(), -1, SQLITE_TRANSIENT);
  sqlite3_bind_text(stmt, 4, snapshot.album.c_str(), -1, SQLITE_TRANSIENT);
  sqlite3_bind_text(stmt, 5, snapshot.genre.c_str(), -1, SQLITE_TRANSIENT);
  sqlite3_bind_text(stmt, 6, snapshot.composer.c_str(), -1, SQLITE_TRANSIENT);
  sqlite3_bind_text(stmt, 7, snapshot.comment.c_str(), -1, SQLITE_TRANSIENT);
  sqlite3_bind_text(stmt, 8, snapshot.sort_title.c_str(), -1, SQLITE_TRANSIENT);
  sqlite3_bind_text(stmt, 9, snapshot.file_kind.c_str(), -1, SQLITE_TRANSIENT);
  sqlite3_bind_text(stmt, 10, snapshot.stream_url.c_str(), -1, SQLITE_TRANSIENT);
  sqlite3_bind_text(stmt, 11, snapshot.track_id.c_str(), -1, SQLITE_TRANSIENT);
  sqlite3_bind_text(stmt, 12, snapshot.persistent_id.c_str(), -1, SQLITE_TRANSIENT);
  sqlite3_bind_text(stmt, 13, snapshot.client_name.c_str(), -1, SQLITE_TRANSIENT);
  sqlite3_bind_text(stmt, 14, snapshot.client_model.c_str(), -1, SQLITE_TRANSIENT);
  sqlite3_bind_text(stmt, 15, snapshot.client_user_agent.c_str(), -1, SQLITE_TRANSIENT);
  sqlite3_bind_text(stmt, 16, snapshot.client_ip.c_str(), -1, SQLITE_TRANSIENT);
  sqlite3_bind_text(stmt, 17, snapshot.client_device_id.c_str(), -1, SQLITE_TRANSIENT);
  sqlite3_bind_text(stmt, 18, snapshot.client_mac.c_str(), -1, SQLITE_TRANSIENT);
  sqlite3_bind_int(stmt, 19, snapshot.playing ? 1 : 0);
  sqlite3_bind_int(stmt, 20, snapshot.position_ms);
  sqlite3_bind_int(stmt, 21, snapshot.duration_ms);
  sqlite3_bind_int(stmt, 22, snapshot.has_cover_art ? 1 : 0);
  sqlite3_bind_text(stmt, 23, snapshot.cover_art_id.c_str(), -1, SQLITE_TRANSIENT);
  sqlite3_bind_text(stmt, 24, cover_path.c_str(), -1, SQLITE_TRANSIENT);
  sqlite3_bind_text(stmt, 25, snapshot.metadata_quality.c_str(), -1, SQLITE_TRANSIENT);
  sqlite3_bind_text(stmt, 26, snapshot.started_at.c_str(), -1, SQLITE_TRANSIENT);
  sqlite3_bind_text(stmt, 27, updated_at.c_str(), -1, SQLITE_TRANSIENT);
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
      "UPDATE audio_now_playing SET owner_zone_id=0, title='', artist='', album='', genre='', "
      "composer='', comment='', sort_title='', file_kind='', stream_url='', track_id='', "
      "persistent_id='', client_name='', client_model='', client_user_agent='', client_ip='', "
      "client_device_id='', client_mac='', playing=0, position_ms=0, duration_ms=0, "
      "has_cover_art=0, cover_art_id='', cover_art_path='', metadata_quality='empty', "
      "started_at='', updated_at=? WHERE id=1;";
  if (sqlite3_prepare_v2(db_->handle(), clear_now_playing, -1, &stmt, nullptr) != SQLITE_OK) {
    return;
  }
  const std::string updated_at = homepi::events::iso_timestamp();
  sqlite3_bind_text(stmt, 1, updated_at.c_str(), -1, SQLITE_TRANSIENT);
  sqlite3_step(stmt);
  sqlite3_finalize(stmt);
}

namespace {

void bind_snapshot_columns(sqlite3_stmt* stmt, NowPlayingSnapshot& snapshot, int owner_col) {
  snapshot.owner_zone_id = sqlite3_column_int(stmt, owner_col);
  if (const unsigned char* text = sqlite3_column_text(stmt, owner_col + 1)) {
    snapshot.title = reinterpret_cast<const char*>(text);
  }
  if (const unsigned char* text = sqlite3_column_text(stmt, owner_col + 2)) {
    snapshot.artist = reinterpret_cast<const char*>(text);
  }
  if (const unsigned char* text = sqlite3_column_text(stmt, owner_col + 3)) {
    snapshot.album = reinterpret_cast<const char*>(text);
  }
  if (const unsigned char* text = sqlite3_column_text(stmt, owner_col + 4)) {
    snapshot.client_name = reinterpret_cast<const char*>(text);
  }
  if (const unsigned char* text = sqlite3_column_text(stmt, owner_col + 5)) {
    snapshot.track_id = reinterpret_cast<const char*>(text);
  }
  snapshot.playing = sqlite3_column_int(stmt, owner_col + 6) != 0;
  snapshot.position_ms = sqlite3_column_int(stmt, owner_col + 7);
  snapshot.duration_ms = sqlite3_column_int(stmt, owner_col + 8);
  snapshot.has_cover_art = sqlite3_column_int(stmt, owner_col + 9) != 0;
}

}  // namespace

std::optional<NowPlayingSnapshot> MetadataStateRepository::load_owner(int owner_zone_id) const {
  if (!db_ || owner_zone_id <= 0) {
    return std::nullopt;
  }
  sqlite3_stmt* stmt = nullptr;
  const char* sql =
      "SELECT title, artist, album, client_name, track_id, playing, position_ms, duration_ms, "
      "has_cover_art, cover_art_id, cover_art_path, persistent_id, client_model, genre, composer, "
      "metadata_quality, started_at "
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
  if (const unsigned char* text = sqlite3_column_text(stmt, 9)) {
    snapshot.cover_art_id = reinterpret_cast<const char*>(text);
  }
  if (const unsigned char* text = sqlite3_column_text(stmt, 10)) {
    snapshot.cover_art_path = reinterpret_cast<const char*>(text);
  }
  if (const unsigned char* text = sqlite3_column_text(stmt, 11)) {
    snapshot.persistent_id = reinterpret_cast<const char*>(text);
  }
  if (const unsigned char* text = sqlite3_column_text(stmt, 12)) {
    snapshot.client_model = reinterpret_cast<const char*>(text);
  }
  if (const unsigned char* text = sqlite3_column_text(stmt, 13)) {
    snapshot.genre = reinterpret_cast<const char*>(text);
  }
  if (const unsigned char* text = sqlite3_column_text(stmt, 14)) {
    snapshot.composer = reinterpret_cast<const char*>(text);
  }
  if (const unsigned char* text = sqlite3_column_text(stmt, 15)) {
    snapshot.metadata_quality = reinterpret_cast<const char*>(text);
  }
  if (const unsigned char* text = sqlite3_column_text(stmt, 16)) {
    snapshot.started_at = reinterpret_cast<const char*>(text);
  }
  sqlite3_finalize(stmt);
  return snapshot;
}

std::optional<NowPlayingSnapshot> MetadataStateRepository::load_now_playing() const {
  if (!db_) {
    return std::nullopt;
  }
  sqlite3_stmt* stmt = nullptr;
  const char* sql =
      "SELECT owner_zone_id, title, artist, album, track_id, client_name, playing, position_ms, "
      "duration_ms, has_cover_art, cover_art_id, cover_art_path, persistent_id, client_model, "
      "metadata_quality, started_at, updated_at "
      "FROM audio_now_playing WHERE id = 1 AND owner_zone_id > 0 LIMIT 1;";
  if (sqlite3_prepare_v2(db_->handle(), sql, -1, &stmt, nullptr) != SQLITE_OK) {
    return std::nullopt;
  }
  if (sqlite3_step(stmt) != SQLITE_ROW) {
    sqlite3_finalize(stmt);
    return std::nullopt;
  }

  NowPlayingSnapshot snapshot;
  bind_snapshot_columns(stmt, snapshot, 0);
  if (const unsigned char* text = sqlite3_column_text(stmt, 10)) {
    snapshot.cover_art_id = reinterpret_cast<const char*>(text);
  }
  if (const unsigned char* text = sqlite3_column_text(stmt, 11)) {
    snapshot.cover_art_path = reinterpret_cast<const char*>(text);
  }
  if (const unsigned char* text = sqlite3_column_text(stmt, 12)) {
    snapshot.persistent_id = reinterpret_cast<const char*>(text);
  }
  if (const unsigned char* text = sqlite3_column_text(stmt, 13)) {
    snapshot.client_model = reinterpret_cast<const char*>(text);
  }
  if (const unsigned char* text = sqlite3_column_text(stmt, 14)) {
    snapshot.metadata_quality = reinterpret_cast<const char*>(text);
  }
  if (const unsigned char* text = sqlite3_column_text(stmt, 15)) {
    snapshot.started_at = reinterpret_cast<const char*>(text);
  }
  if (const unsigned char* text = sqlite3_column_text(stmt, 16)) {
    snapshot.updated_at = reinterpret_cast<const char*>(text);
  }
  sqlite3_finalize(stmt);
  return snapshot;
}

std::string MetadataStateRepository::write_cover_art(const std::string& cache_dir, int zone_id,
                                                     const std::vector<std::uint8_t>& bytes,
                                                     const std::string& existing_art_id) {
  if (zone_id <= 0 || bytes.empty()) {
    return {};
  }

  const std::string art_id = sha256_hex(bytes);
  if (!existing_art_id.empty() && existing_art_id == art_id) {
    return art_id;
  }

  const std::string art_dir = artwork_directory(cache_dir);
  std::error_code ec;
  fs::create_directories(art_dir, ec);

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

void MetadataStateRepository::cache_track_duration(const std::string& track_key, int duration_ms) {
  if (!db_ || track_key.empty() || duration_ms <= 0) {
    return;
  }
  sqlite3_stmt* stmt = nullptr;
  const char* sql =
      "INSERT INTO track_duration_cache (track_key, duration_ms, updated_at) "
      "VALUES (?, ?, ?) "
      "ON CONFLICT(track_key) DO UPDATE SET "
      "duration_ms=excluded.duration_ms, updated_at=excluded.updated_at;";
  if (sqlite3_prepare_v2(db_->handle(), sql, -1, &stmt, nullptr) != SQLITE_OK) {
    return;
  }
  const std::string updated_at = homepi::events::iso_timestamp();
  sqlite3_bind_text(stmt, 1, track_key.c_str(), -1, SQLITE_TRANSIENT);
  sqlite3_bind_int(stmt, 2, duration_ms);
  sqlite3_bind_text(stmt, 3, updated_at.c_str(), -1, SQLITE_TRANSIENT);
  sqlite3_step(stmt);
  sqlite3_finalize(stmt);
}

std::optional<int> MetadataStateRepository::load_cached_track_duration(
    const std::string& track_key) const {
  if (!db_ || track_key.empty()) {
    return std::nullopt;
  }
  sqlite3_stmt* stmt = nullptr;
  const char* sql =
      "SELECT duration_ms FROM track_duration_cache WHERE track_key = ? LIMIT 1;";
  if (sqlite3_prepare_v2(db_->handle(), sql, -1, &stmt, nullptr) != SQLITE_OK) {
    return std::nullopt;
  }
  sqlite3_bind_text(stmt, 1, track_key.c_str(), -1, SQLITE_TRANSIENT);
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

std::optional<int> MetadataStateRepository::record_play_history(
    const NowPlayingSnapshot& snapshot) {
  if (!db_ || snapshot.owner_zone_id <= 0 || !is_meaningful_stream(snapshot)) {
    return std::nullopt;
  }

  std::lock_guard lock(db_mutex_);

  const std::string history_track_id =
      is_persistent_id_like(snapshot.track_id) ? snapshot.track_id : snapshot.persistent_id;
  const std::string history_persistent_id =
      is_persistent_id_like(snapshot.persistent_id) ? snapshot.persistent_id : history_track_id;
  const std::string track_key =
      history_track_id.empty()
          ? snapshot.title + "|" + snapshot.artist + "|" + std::to_string(snapshot.owner_zone_id)
          : history_track_id;
  const std::string ended_at = homepi::events::iso_timestamp();
  const std::string started_at =
      snapshot.started_at.empty() ? ended_at : snapshot.started_at;
  const int last_position =
      snapshot.duration_ms > 0 ? snapshot.duration_ms : snapshot.position_ms;
  const int duration = snapshot.duration_ms > 0 ? snapshot.duration_ms : snapshot.position_ms;
  const std::string cover_path = snapshot.cover_art_path.empty() && snapshot.has_cover_art
                                     ? artwork_directory(cache_dir_) + "/sha256-" +
                                           snapshot.cover_art_id + ".jpg"
                                     : snapshot.cover_art_path;

  sqlite3_stmt* stmt = nullptr;
  const char* insert_sql =
      "INSERT INTO audio_play_history "
      "(stream_key, source, zone_id, title, artist, album, track_id, persistent_id, client_name, "
      "client_model, client_ip, duration_ms, last_position_ms, has_cover_art, cover_art_id, "
      "cover_art_path, started_at, ended_at, created_at, played_at) "
      "VALUES (?, 'airplay', ?, ?, ?, ?, ?, ?, ?, ?, ?, ?, ?, ?, ?, ?, ?, ?, ?, ?);";
  if (sqlite3_prepare_v2(db_->handle(), insert_sql, -1, &stmt, nullptr) != SQLITE_OK) {
    return std::nullopt;
  }
  sqlite3_bind_text(stmt, 1, track_key.c_str(), -1, SQLITE_TRANSIENT);
  sqlite3_bind_int(stmt, 2, snapshot.owner_zone_id);
  sqlite3_bind_text(stmt, 3, snapshot.title.c_str(), -1, SQLITE_TRANSIENT);
  sqlite3_bind_text(stmt, 4, snapshot.artist.c_str(), -1, SQLITE_TRANSIENT);
  sqlite3_bind_text(stmt, 5, snapshot.album.c_str(), -1, SQLITE_TRANSIENT);
  sqlite3_bind_text(stmt, 6, history_track_id.c_str(), -1, SQLITE_TRANSIENT);
  sqlite3_bind_text(stmt, 7, history_persistent_id.c_str(), -1, SQLITE_TRANSIENT);
  sqlite3_bind_text(stmt, 8, snapshot.client_name.c_str(), -1, SQLITE_TRANSIENT);
  sqlite3_bind_text(stmt, 9, snapshot.client_model.c_str(), -1, SQLITE_TRANSIENT);
  sqlite3_bind_text(stmt, 10, snapshot.client_ip.c_str(), -1, SQLITE_TRANSIENT);
  sqlite3_bind_int(stmt, 11, duration);
  sqlite3_bind_int(stmt, 12, last_position);
  sqlite3_bind_int(stmt, 13, snapshot.has_cover_art ? 1 : 0);
  sqlite3_bind_text(stmt, 14, snapshot.cover_art_id.c_str(), -1, SQLITE_TRANSIENT);
  sqlite3_bind_text(stmt, 15, cover_path.c_str(), -1, SQLITE_TRANSIENT);
  sqlite3_bind_text(stmt, 16, started_at.c_str(), -1, SQLITE_TRANSIENT);
  sqlite3_bind_text(stmt, 17, ended_at.c_str(), -1, SQLITE_TRANSIENT);
  sqlite3_bind_text(stmt, 18, ended_at.c_str(), -1, SQLITE_TRANSIENT);
  sqlite3_bind_text(stmt, 19, ended_at.c_str(), -1, SQLITE_TRANSIENT);
  if (sqlite3_step(stmt) != SQLITE_DONE) {
    sqlite3_finalize(stmt);
    return std::nullopt;
  }
  sqlite3_finalize(stmt);

  const int inserted_id = static_cast<int>(sqlite3_last_insert_rowid(db_->handle()));

  sqlite3_exec(db_->handle(),
               "DELETE FROM audio_play_history WHERE id NOT IN "
               "(SELECT id FROM audio_play_history ORDER BY ended_at DESC, id DESC LIMIT 20);",
               nullptr, nullptr, nullptr);

  return inserted_id;
}

std::vector<PlayHistoryEntry> MetadataStateRepository::load_play_history(int limit) const {
  std::vector<PlayHistoryEntry> entries;
  if (!db_ || limit <= 0) {
    return entries;
  }

  sqlite3_stmt* stmt = nullptr;
  const char* sql =
      "SELECT id, zone_id, title, artist, album, track_id, persistent_id, client_name, client_model, "
      "duration_ms, last_position_ms, has_cover_art, cover_art_id, cover_art_path, started_at, "
      "ended_at, played_at "
      "FROM audio_play_history ORDER BY ended_at DESC, id DESC LIMIT ?;";
  if (sqlite3_prepare_v2(db_->handle(), sql, -1, &stmt, nullptr) != SQLITE_OK) {
    return entries;
  }
  sqlite3_bind_int(stmt, 1, limit);
  while (sqlite3_step(stmt) == SQLITE_ROW) {
    PlayHistoryEntry entry;
    entry.id = sqlite3_column_int(stmt, 0);
    entry.zone_id = sqlite3_column_int(stmt, 1);
    if (const char* text = reinterpret_cast<const char*>(sqlite3_column_text(stmt, 2))) {
      entry.title = text;
    }
    if (const char* text = reinterpret_cast<const char*>(sqlite3_column_text(stmt, 3))) {
      entry.artist = text;
    }
    if (const char* text = reinterpret_cast<const char*>(sqlite3_column_text(stmt, 4))) {
      entry.album = text;
    }
    if (const char* text = reinterpret_cast<const char*>(sqlite3_column_text(stmt, 5))) {
      entry.track_id = text;
    }
    if (const char* text = reinterpret_cast<const char*>(sqlite3_column_text(stmt, 6))) {
      entry.persistent_id = text;
    }
    if (const char* text = reinterpret_cast<const char*>(sqlite3_column_text(stmt, 7))) {
      entry.client_name = text;
    }
    if (const char* text = reinterpret_cast<const char*>(sqlite3_column_text(stmt, 8))) {
      entry.client_model = text;
    }
    entry.duration_ms = sqlite3_column_int(stmt, 9);
    entry.last_position_ms = sqlite3_column_int(stmt, 10);
    entry.has_cover_art = sqlite3_column_int(stmt, 11) != 0;
    if (const char* text = reinterpret_cast<const char*>(sqlite3_column_text(stmt, 12))) {
      entry.cover_art_id = text;
    }
    if (const char* text = reinterpret_cast<const char*>(sqlite3_column_text(stmt, 13))) {
      entry.cover_art_path = text;
    }
    if (const char* text = reinterpret_cast<const char*>(sqlite3_column_text(stmt, 14))) {
      entry.started_at = text;
    }
    if (const char* text = reinterpret_cast<const char*>(sqlite3_column_text(stmt, 15))) {
      entry.ended_at = text;
    }
    if (const char* text = reinterpret_cast<const char*>(sqlite3_column_text(stmt, 16))) {
      entry.played_at = text;
    }
    if (entry.played_at.empty()) {
      entry.played_at = entry.ended_at;
    }
    entries.push_back(std::move(entry));
  }
  sqlite3_finalize(stmt);
  return entries;
}

int MetadataStateRepository::cleanup_unreferenced_artwork() const {
  if (!db_) {
    return 0;
  }

  std::unordered_set<std::string> keep_ids;
  for (const std::string& art_id : referenced_cover_art_ids()) {
    if (!art_id.empty()) {
      keep_ids.insert(art_id);
    }
  }

  const std::string art_dir = artwork_directory(cache_dir_);
  std::error_code ec;
  if (!fs::exists(art_dir, ec)) {
    return 0;
  }

  int removed = 0;
  for (const auto& path : fs::directory_iterator(art_dir, ec)) {
    if (!path.is_regular_file()) {
      continue;
    }
    const std::string filename = path.path().filename().string();
    if (filename == "current.jpg") {
      continue;
    }
    const std::string prefix = "sha256-";
    if (filename.rfind(prefix, 0) != 0) {
      continue;
    }
    std::string art_id = filename.substr(prefix.size());
    if (art_id.size() > 4 && art_id.substr(art_id.size() - 4) == ".jpg") {
      art_id = art_id.substr(0, art_id.size() - 4);
    }
    if (keep_ids.contains(art_id)) {
      continue;
    }
    if (fs::remove(path.path(), ec)) {
      ++removed;
    }
  }
  return removed;
}

std::vector<std::string> MetadataStateRepository::referenced_cover_art_ids() const {
  std::vector<std::string> ids;
  if (const auto now_playing = load_now_playing()) {
    if (!now_playing->cover_art_id.empty()) {
      ids.push_back(now_playing->cover_art_id);
    }
  }
  for (const auto& entry : load_play_history(20)) {
    if (!entry.cover_art_id.empty()) {
      ids.push_back(entry.cover_art_id);
    }
  }
  return ids;
}

void MetadataStateRepository::update_latest_history_cover(const std::string& track_id,
                                                          const std::string& cover_art_id,
                                                          const std::string& cover_art_path) {
  if (!db_ || track_id.empty() || cover_art_id.empty()) {
    return;
  }
  sqlite3_stmt* stmt = nullptr;
  const char* sql =
      "UPDATE audio_play_history SET has_cover_art = 1, cover_art_id = ?, cover_art_path = ? "
      "WHERE id = (SELECT id FROM audio_play_history WHERE track_id = ? "
      "ORDER BY ended_at DESC, id DESC LIMIT 1);";
  if (sqlite3_prepare_v2(db_->handle(), sql, -1, &stmt, nullptr) != SQLITE_OK) {
    return;
  }
  sqlite3_bind_text(stmt, 1, cover_art_id.c_str(), -1, SQLITE_TRANSIENT);
  sqlite3_bind_text(stmt, 2, cover_art_path.c_str(), -1, SQLITE_TRANSIENT);
  sqlite3_bind_text(stmt, 3, track_id.c_str(), -1, SQLITE_TRANSIENT);
  sqlite3_step(stmt);
  sqlite3_finalize(stmt);
}

}  // namespace homepi::metadata
