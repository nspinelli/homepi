#pragma once

#include <cstdint>
#include <memory>
#include <optional>
#include <string>
#include <vector>

#include "homepi/metadata/now-playing-state.hpp"

namespace homepi::storage {
class DatabaseConnection;
}

namespace homepi::metadata {

/**
 * Persists now-playing metadata for the PCM owner zone using core/storage.
 */
class MetadataStateRepository {
 public:
  /**
   * Opens the metadata database and applies migrations.
   * @param database_path SQLite database path.
   */
  explicit MetadataStateRepository(const std::string& database_path);
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
   * @returns Cover art id (sha256 hex) when written.
   */
  static std::string write_cover_art(const std::string& cache_dir, int zone_id,
                                     const std::vector<std::uint8_t>& bytes);

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
   * Stores a learned track duration keyed by AirPlay track id.
   * @param track_id Persistent track identifier.
   * @param duration_ms Duration in milliseconds.
   */
  void cache_track_duration(const std::string& track_id, int duration_ms);

  /**
   * Loads a previously learned duration for a track id.
   * @param track_id Persistent track identifier.
   * @returns Duration in milliseconds when cached.
   */
  std::optional<int> load_cached_track_duration(const std::string& track_id) const;

  /**
   * Inserts a completed stream into play history and trims to the last 20 rows.
   * @param snapshot Final snapshot for the stream.
   */
  void record_play_history(const NowPlayingSnapshot& snapshot);

 private:
  std::string database_path_;
  std::unique_ptr<homepi::storage::DatabaseConnection> db_;
};

}  // namespace homepi::metadata
