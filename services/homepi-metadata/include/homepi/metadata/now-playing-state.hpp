#pragma once

#include <mutex>
#include <string>

namespace homepi::metadata {

/** In-memory now-playing metadata for the PCM owner zone (spec §11.2). */
struct NowPlayingSnapshot {
  int owner_zone_id = 0;

  std::string title;
  std::string artist;
  std::string album;
  std::string genre;
  std::string composer;
  std::string comment;
  std::string sort_title;
  std::string file_kind;
  std::string stream_url;

  std::string track_id;
  std::string persistent_id;

  std::string client_name;
  std::string client_model;
  std::string client_user_agent;
  std::string client_ip;
  std::string client_device_id;
  std::string client_mac;

  bool playing = false;
  int position_ms = 0;
  int duration_ms = 0;

  bool has_cover_art = false;
  std::string cover_art_id;
  std::string cover_art_path;

  std::string metadata_quality = "empty";
  std::string started_at;
  std::string updated_at;
};

/**
 * Thread-safe now-playing state for the active PCM owner zone.
 */
class NowPlayingState {
 public:
  /**
   * Replaces the owner zone and clears track metadata when the owner changes.
   * @param owner_zone_id New owner zone id.
   * @returns True when the owner changed.
   */
  bool set_owner_zone(int owner_zone_id);

  /**
   * Assigns the owner zone when none is set yet, without clearing track metadata.
   * @param owner_zone_id New owner zone id.
   * @returns True when the owner zone was claimed.
   */
  bool claim_owner_zone(int owner_zone_id);

  /**
   * Updates a single metadata field for the owner zone.
   * @param zone_id Source zone id.
   * @param field Field name.
   * @param value New value.
   * @returns True when the snapshot changed.
   */
  bool update_field(int zone_id, const std::string& field, const std::string& value);

  /**
   * Sets started_at to the current ISO timestamp for a new track.
   */
  void touch_started_at();

  /**
   * Applies title, artist, and album together for one track identity.
   * @param zone_id Owner zone id.
   * @param track_id Track persistent id for this metadata group.
   * @param title Track title.
   * @param artist Track artist.
   * @param album Track album.
   * @returns True when any field changed.
   */
  bool apply_track_metadata(int zone_id, const std::string& track_id, const std::string& title,
                            const std::string& artist, const std::string& album);

  /**
   * Updates playback progress for the owner zone.
   * @param zone_id Source zone id.
   * @param position_ms Current position in milliseconds (-1 to keep).
   * @param duration_ms Track duration in milliseconds (-1 to keep).
   * @param playing Whether playback is active.
   * @returns True when the snapshot changed.
   */
  bool update_progress(int zone_id, int position_ms, int duration_ms, bool playing);

  /**
   * Updates playback state for the owner zone.
   * @param zone_id Source zone id.
   * @param playing Whether playback is active.
   * @returns True when the snapshot changed.
   */
  bool update_playing(int zone_id, bool playing);

  /**
   * Marks cover art as available for the owner zone.
   * @param zone_id Source zone id.
   * @param cover_art_id Content hash id.
   * @param cover_art_path Filesystem path to cached artwork.
   * @param force When true, re-mark even if cover art was already present.
   * @returns True when the snapshot changed.
   */
  bool mark_cover_art(int zone_id, const std::string& cover_art_id,
                      const std::string& cover_art_path, bool force = false);

  /**
   * Clears track identity fields when a new persistent id starts a bundle.
   * Preserves client/session fields.
   */
  void clear_track_identity();

  /** Clears bundle text fields at mdst without removing session client fields. */
  void begin_metadata_bundle();

  /** Clears duration and cover flags when the title changes without a new bundle. */
  void prepare_title_only_change();

  /** Clears track text fields while preserving playback timing. */
  void clear_metadata_fields();

  /** Clears track metadata while preserving the owner zone. */
  void clear_track_metadata();

  /** Clears metadata for the current owner zone. */
  void clear();

  /**
   * Restores the metadata timestamp from persistence when memory is empty.
   * @param updated_at ISO timestamp from the database.
   */
  void restore_updated_at(const std::string& updated_at);

  /** Applies display normalization and metadata_quality scoring. */
  void normalize_snapshot();

  /**
   * Returns a copy of the current snapshot.
   * @returns Now-playing snapshot.
   */
  NowPlayingSnapshot snapshot() const;

  /**
   * Returns a copy of the snapshot for stream finalization without clearing state.
   * @returns Current snapshot.
   */
  NowPlayingSnapshot snapshot_for_finalize() const;

  /**
   * Replaces in-memory track metadata from a persisted database row.
   * @param persisted Persisted now-playing row.
   */
  void apply_persisted_snapshot(const NowPlayingSnapshot& persisted);

 private:
  void refresh_metadata_quality_locked();
  void touch_metadata_timestamp_locked();

  mutable std::mutex mutex_;
  NowPlayingSnapshot snapshot_;
};

}  // namespace homepi::metadata
