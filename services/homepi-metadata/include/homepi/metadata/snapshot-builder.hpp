#pragma once

#include <string>

#include "homepi/metadata/now-playing-state.hpp"

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
   * Serializes a metadata field update payload.
   * @param zone_id Zone id.
   * @param field Field name.
   * @param value Field value.
   * @returns JSON object string.
   */
  static std::string build_field_payload(int zone_id, const std::string& field,
                                         const std::string& value);

  /**
   * Serializes a progress update payload.
   * @param zone_id Zone id.
   * @param position_ms Position in milliseconds.
   * @param duration_ms Duration in milliseconds.
   * @param playing Whether playback is active.
   * @returns JSON object string.
   */
  static std::string build_progress_payload(int zone_id, int position_ms, int duration_ms,
                                            bool playing);

  /**
   * Serializes a cover-art update payload.
   * @param zone_id Zone id.
   * @returns JSON object string.
   */
  static std::string build_cover_payload(int zone_id);
};

}  // namespace homepi::metadata
