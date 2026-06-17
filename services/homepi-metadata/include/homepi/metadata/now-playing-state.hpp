#pragma once

#include <mutex>
#include <optional>
#include <string>

namespace homepi::metadata {

/** In-memory now-playing metadata for the PCM owner zone. */
struct NowPlayingSnapshot {
  int owner_zone_id = 0;
  std::string title;
  std::string artist;
  std::string album;
  std::string client_name;
  bool playing = false;
  int position_ms = 0;
  int duration_ms = 0;
  bool has_cover_art = false;
};

/**
 * Thread-safe now-playing state for the active PCM owner zone.
 */
class NowPlayingState {
 public:
  /**
   * Replaces the owner zone and clears metadata when the owner changes.
   * @param owner_zone_id New owner zone id.
   * @returns True when the owner changed.
   */
  bool set_owner_zone(int owner_zone_id);

  /**
   * Updates one metadata field for the owner zone.
   * @param zone_id Source zone id.
   * @param field Metadata field name.
   * @param value Field value.
   * @returns True when the snapshot changed.
   */
  bool update_field(int zone_id, const std::string& field, const std::string& value);

  /**
   * Updates playback progress for the owner zone.
   * @param zone_id Source zone id.
   * @param position_ms Current position in milliseconds.
   * @param duration_ms Track duration in milliseconds.
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
   * @param force When true, re-mark even if cover art was already present.
   * @returns True when the snapshot changed.
   */
  bool mark_cover_art(int zone_id, bool force = false);

  /**
   * Clears track metadata while preserving the owner zone.
   */
  void clear_track_metadata();

  /**
   * Clears metadata for the current owner zone.
   */
  void clear();

  /**
   * Returns a copy of the current snapshot.
   * @returns Now-playing snapshot.
   */
  NowPlayingSnapshot snapshot() const;

 private:
  mutable std::mutex mutex_;
  NowPlayingSnapshot snapshot_;
};

}  // namespace homepi::metadata
