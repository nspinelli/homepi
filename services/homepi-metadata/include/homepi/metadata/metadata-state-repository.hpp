#pragma once

#include <cstdint>
#include <memory>
#include <mutex>
#include <optional>
#include <string>
#include <vector>

#include "homepi/metadata/now-playing-state.hpp"

namespace homepi::storage {
class DatabaseConnection;
}

namespace homepi::metadata {

/** One row from the last-20 play history table. */
struct PlayHistoryEntry {
  int id = 0;
  int zone_id = 0;
  std::string title;
  std::string artist;
  std::string album;
  std::string track_id;
  std::string persistent_id;
  std::string client_name;
  std::string client_model;
  int duration_ms = 0;
  int last_position_ms = 0;
  bool has_cover_art = false;
  std::string cover_art_id;
  std::string cover_art_path;
  std::string started_at;
  std::string ended_at;
  std::string played_at;
};

/**
 * Persists now-playing metadata for the PCM owner zone using core/storage.
 */
class MetadataStateRepository {
 public:
  /**
   * Opens the metadata database and applies migrations.
   * @param database_path SQLite database path.
   * @param cache_dir Cache directory for artwork paths.
   */
  explicit MetadataStateRepository(const std::string& database_path,
                                   const std::string& cache_dir);
  ~MetadataStateRepository();

  MetadataStateRepository(const MetadataStateRepository&) = delete;
  MetadataStateRepository& operator=(const MetadataStateRepository&) = delete;

  /**
   * Saves the owner-zone snapshot.
   * @param snapshot Snapshot to persist.
   */
  void save_snapshot(const NowPlayingSnapshot& snapshot);

  /**
   * Clears persisted metadata for an owner zone.
   * @param owner_zone_id Owner zone id.
   */
  void clear_owner(int owner_zone_id);

  /**
   * Loads the latest persisted snapshot for an owner zone.
   * @param owner_zone_id Owner zone id.
   * @returns Snapshot when present.
   */
  std::optional<NowPlayingSnapshot> load_owner(int owner_zone_id) const;

  /**
   * Writes cover art bytes to the content-addressed artwork cache.
   * @param cache_dir Cache directory path.
   * @param zone_id Owner zone id for legacy cover-zone path.
   * @param bytes Cover art bytes.
   * @param existing_art_id Skip rewrite when bytes hash matches this id.
   * @returns Cover art id (sha256 hex) when written or unchanged.
   */
  static std::string write_cover_art(const std::string& cache_dir, int zone_id,
                                     const std::vector<std::uint8_t>& bytes,
                                     const std::string& existing_art_id = "");

  /**
   * Returns the canonical artwork directory under the cache root.
   * @param cache_dir Cache directory path.
   * @returns Path to metadata/artwork.
   */
  static std::string artwork_directory(const std::string& cache_dir);

  /**
   * Removes cached cover art for a zone when metadata is cleared.
   * @param cache_dir Cache directory path.
   * @param zone_id Zone id.
   * @returns True when the file was removed or absent.
   */
  static bool delete_cover_art(const std::string& cache_dir, int zone_id);

  /**
   * Stores a learned track duration keyed by track id or persistent id.
   * @param track_key Track key (track_id or persistent_id).
   * @param duration_ms Duration in milliseconds.
   */
  void cache_track_duration(const std::string& track_key, int duration_ms);

  /**
   * Loads a previously learned duration for a track key.
   * @param track_key Track key (track_id or persistent_id).
   * @returns Duration in milliseconds when cached.
   */
  std::optional<int> load_cached_track_duration(const std::string& track_key) const;

  /**
   * Loads the global now-playing row when present.
   * @returns Snapshot from audio_now_playing.
   */
  std::optional<NowPlayingSnapshot> load_now_playing() const;

  /**
   * Returns whether a stream snapshot is meaningful enough for history.
   * @param snapshot Snapshot to evaluate.
   * @returns True when at least one meaningful field is present.
   */
  static bool is_meaningful_stream(const NowPlayingSnapshot& snapshot);

  /**
   * Inserts a completed stream into play history and trims to the last 20 rows.
   * @param snapshot Final snapshot for the stream.
   * @returns Inserted row id when recorded.
   */
  std::optional<int> record_play_history(const NowPlayingSnapshot& snapshot);

  /**
   * Loads recent play history rows newest-first.
   * @param limit Maximum rows to return.
   * @returns History entries.
   */
  std::vector<PlayHistoryEntry> load_play_history(int limit = 20) const;

  /**
   * Returns cover art ids referenced by now-playing and recent history.
   * @returns Cover art ids that must be retained on disk.
   */
  std::vector<std::string> referenced_cover_art_ids() const;

  /**
   * Updates cover metadata on the most recent history row for a track.
   * @param track_id Track persistent id.
   * @param cover_art_id Content hash id.
   * @param cover_art_path Absolute artwork path.
   */
  void update_latest_history_cover(const std::string& track_id, const std::string& cover_art_id,
                                   const std::string& cover_art_path);

  /**
   * Deletes artwork files not referenced by current now-playing or recent history.
   * @returns Number of files removed.
   */
  int cleanup_unreferenced_artwork() const;

  /**
   * Returns the preferred track key for duration caching.
   * @param snapshot Snapshot containing track identifiers.
   * @returns track_id, persistent_id, or empty.
   */
  static std::string track_key_for(const NowPlayingSnapshot& snapshot);

 private:
  void apply_column_migrations();

  std::string database_path_;
  std::string cache_dir_;
  std::unique_ptr<homepi::storage::DatabaseConnection> db_;
  mutable std::mutex db_mutex_;
};

}  // namespace homepi::metadata
