#pragma once

#include <string>

namespace homepi::metadata {

/** Cached now-playing fields parsed from a zone metadata pipe. */
struct ZoneMetadataCache {
  std::string title;
  std::string artist;
  std::string album;
  std::string sort_title;
  std::string track_id;
  std::string client_name;
  std::string client_model;
  bool has_cover_art = false;
  bool playing = false;
  int position_ms = 0;
  int duration_ms = 0;
};

/**
 * Per-zone metadata cache used to replay pipe fields when PCM assigns an owner.
 */
class ZoneMetadataCacheStore {
 public:
  /**
   * Returns the cache entry for a zone, creating it when missing.
   * @param zone_id Zone number.
   * @returns Mutable cache entry.
   */
  ZoneMetadataCache& zone(int zone_id);

  /**
   * Returns a zone cache entry when present.
   * @param zone_id Zone number.
   * @returns Cache entry or null when absent.
   */
  const ZoneMetadataCache* find(int zone_id) const;

  /**
   * Marks the start of a Shairport metadata bundle without clearing cached fields.
   * @param zone_id Zone number.
   */
  void begin_bundle(int zone_id);

  /**
   * Clears all cached fields for a zone session end.
   * @param zone_id Zone number.
   */
  void clear_zone(int zone_id);

 private:
  ZoneMetadataCache caches_[17]{};
};

}  // namespace homepi::metadata
