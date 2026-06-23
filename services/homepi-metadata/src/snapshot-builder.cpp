#include "homepi/metadata/snapshot-builder.hpp"

#include "homepi/metadata/json-utils.hpp"

#include <sstream>

namespace homepi::metadata {

std::string SnapshotBuilder::build_payload(const NowPlayingSnapshot& snapshot) {
  std::ostringstream out;
  out << "{"
      << "\"ownerZoneId\":" << snapshot.owner_zone_id << ","
      << "\"zoneId\":" << snapshot.owner_zone_id << ","
      << "\"title\":\"" << escape_json_string(snapshot.title) << "\","
      << "\"artist\":\"" << escape_json_string(snapshot.artist) << "\","
      << "\"album\":\"" << escape_json_string(snapshot.album) << "\","
      << "\"clientName\":\"" << escape_json_string(snapshot.client_name) << "\","
      << "\"playing\":" << (snapshot.playing ? "true" : "false") << ","
      << "\"positionMs\":" << snapshot.position_ms << ","
      << "\"durationMs\":" << snapshot.duration_ms << ","
      << "\"trackId\":\"" << escape_json_string(snapshot.track_id) << "\","
      << "\"hasCoverArt\":" << (snapshot.has_cover_art ? "true" : "false")
      << '}';
  return out.str();
}

std::string SnapshotBuilder::build_field_payload(int zone_id, const std::string& field,
                                                 const std::string& value) {
  std::ostringstream out;
  out << "{"
      << "\"zoneId\":" << zone_id << ","
      << "\"field\":\"" << escape_json_string(field) << "\","
      << "\"value\":\"" << escape_json_string(value) << "\""
      << '}';
  return out.str();
}

std::string SnapshotBuilder::build_progress_payload(int zone_id, int position_ms, int duration_ms,
                                                    bool playing) {
  std::ostringstream out;
  out << "{"
      << "\"zoneId\":" << zone_id << ","
      << "\"positionMs\":" << position_ms << ","
      << "\"durationMs\":" << duration_ms << ","
      << "\"playing\":" << (playing ? "true" : "false")
      << '}';
  return out.str();
}

std::string SnapshotBuilder::build_cover_payload(int zone_id) {
  std::ostringstream out;
  out << "{\"zoneId\":" << zone_id << ",\"hasCoverArt\":true}";
  return out.str();
}

}  // namespace homepi::metadata
