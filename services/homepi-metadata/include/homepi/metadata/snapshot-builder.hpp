#pragma once

#include <string>
#include <vector>

#include "homepi/metadata/now-playing-state.hpp"
#include "homepi/metadata/metadata-state-repository.hpp"

namespace homepi::metadata {

/**
 * Builds JSON payloads for metadata snapshots and field updates.
 */
class SnapshotBuilder {
 public:
  /**
   * Serializes a now-playing snapshot payload.
   * @param snapshot Snapshot to serialize.
   * @returns JSON object string.
   */
  static std::string build_payload(const NowPlayingSnapshot& snapshot);

  /**
   * Serializes a metadata owner changed payload.
   * @param owner_zone_id New owner zone id.
   * @param previous_owner_zone_id Previous owner zone id.
   * @returns JSON object string.
   */
  static std::string build_owner_changed_payload(int owner_zone_id, int previous_owner_zone_id);

  /**
   * Serializes a metadata field update payload.
   * @param zone_id Zone id.
   * @param field Field name.
   * @param value Field value.
   * @returns JSON object string.
   */
  static std::string build_field_payload(int zone_id, const std::string& field,
                                         const std::string& value);

  /**
   * Serializes a progress update payload (spec §16.4).
   * @param zone_id Zone id.
   * @param position_ms Position in milliseconds.
   * @param duration_ms Duration in milliseconds.
   * @param playing Whether playback is active.
   * @returns JSON object string.
   */
  static std::string build_progress_payload(int zone_id, int position_ms, int duration_ms,
                                            bool playing);

  /**
   * Serializes a cover-art update payload (spec §16.3).
   * @param snapshot Current snapshot with cover metadata.
   * @returns JSON object string.
   */
  static std::string build_cover_payload(const NowPlayingSnapshot& snapshot);

  /**
   * Serializes a play-history list payload.
   * @param entries History rows newest-first.
   * @returns JSON object string.
   */
  static std::string build_history_payload(const std::vector<PlayHistoryEntry>& entries);

  /**
   * Serializes a play-history updated notification (spec §16.5).
   * @param entry Latest history row.
   * @param limit History list limit.
   * @returns JSON object string.
   */
  static std::string build_history_updated_payload(const PlayHistoryEntry& entry, int limit);
};

}  // namespace homepi::metadata
