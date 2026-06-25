#include "homepi/metadata/zone-metadata-cache.hpp"

namespace homepi::metadata {

ZoneMetadataCache& ZoneMetadataCacheStore::zone(int zone_id) {
  if (zone_id < 1 || zone_id > 16) {
    return caches_[0];
  }
  return caches_[zone_id];
}

const ZoneMetadataCache* ZoneMetadataCacheStore::find(int zone_id) const {
  if (zone_id < 1 || zone_id > 16) {
    return nullptr;
  }
  return &caches_[zone_id];
}

void ZoneMetadataCacheStore::begin_bundle(int zone_id) {
  (void)zone_id;
  // Shairport may open several mdst/mden sequences per track; keep cached fields.
}

void ZoneMetadataCacheStore::clear_zone(int zone_id) {
  if (zone_id < 1 || zone_id > 16) {
    return;
  }
  caches_[zone_id] = ZoneMetadataCache{};
}

}  // namespace homepi::metadata
