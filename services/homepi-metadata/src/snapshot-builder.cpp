#include "homepi/metadata/snapshot-builder.hpp"

#include "homepi/metadata/json-utils.hpp"

#include <sstream>

namespace homepi::metadata {

namespace {

std::string optional_json_string(const std::string& value) {
  return escape_json_string(value);
}

std::string cover_art_url(const NowPlayingSnapshot& snapshot) {
  if (!snapshot.has_cover_art || snapshot.cover_art_id.empty()) {
    return "";
  }
  return "/api/audio/now-playing/cover?v=sha256-" + snapshot.cover_art_id;
}

}  // namespace

std::string SnapshotBuilder::build_payload(const NowPlayingSnapshot& snapshot) {
  std::ostringstream out;
  out << "{"
      << "\"ownerZoneId\":" << snapshot.owner_zone_id << ","
      << "\"zoneId\":" << snapshot.owner_zone_id << ","
      << "\"title\":\"" << optional_json_string(snapshot.title) << "\","
      << "\"artist\":\"" << optional_json_string(snapshot.artist) << "\","
      << "\"album\":\"" << optional_json_string(snapshot.album) << "\","
      << "\"genre\":\"" << optional_json_string(snapshot.genre) << "\","
      << "\"composer\":\"" << optional_json_string(snapshot.composer) << "\","
      << "\"comment\":\"" << optional_json_string(snapshot.comment) << "\","
      << "\"sortTitle\":\"" << optional_json_string(snapshot.sort_title) << "\","
      << "\"fileKind\":\"" << optional_json_string(snapshot.file_kind) << "\","
      << "\"streamUrl\":\"" << optional_json_string(snapshot.stream_url) << "\","
      << "\"clientName\":\"" << optional_json_string(snapshot.client_name) << "\","
      << "\"clientModel\":\"" << optional_json_string(snapshot.client_model) << "\","
      << "\"clientUserAgent\":\"" << optional_json_string(snapshot.client_user_agent) << "\","
      << "\"clientIp\":\"" << optional_json_string(snapshot.client_ip) << "\","
      << "\"clientDeviceId\":\"" << optional_json_string(snapshot.client_device_id) << "\","
      << "\"clientMac\":\"" << optional_json_string(snapshot.client_mac) << "\","
      << "\"playing\":" << (snapshot.playing ? "true" : "false") << ","
      << "\"positionMs\":" << snapshot.position_ms << ","
      << "\"durationMs\":" << snapshot.duration_ms << ","
      << "\"trackId\":\"" << optional_json_string(snapshot.track_id) << "\","
      << "\"persistentId\":\"" << optional_json_string(snapshot.persistent_id) << "\","
      << "\"hasCoverArt\":" << (snapshot.has_cover_art ? "true" : "false") << ","
      << "\"coverArtId\":\"" << optional_json_string(snapshot.cover_art_id) << "\","
      << "\"coverArtPath\":\"" << optional_json_string(snapshot.cover_art_path) << "\","
      << "\"coverArtUrl\":\"" << optional_json_string(cover_art_url(snapshot)) << "\","
      << "\"metadataQuality\":\"" << optional_json_string(snapshot.metadata_quality) << "\","
      << "\"startedAt\":\"" << optional_json_string(snapshot.started_at) << "\","
      << "\"updatedAt\":\"" << optional_json_string(snapshot.updated_at) << "\""
      << '}';
  return out.str();
}

std::string SnapshotBuilder::build_owner_changed_payload(int owner_zone_id,
                                                         int previous_owner_zone_id) {
  std::ostringstream out;
  out << "{"
      << "\"ownerZoneId\":" << owner_zone_id << ","
      << "\"previousOwnerZoneId\":" << previous_owner_zone_id
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
      << "\"ownerZoneId\":" << zone_id << ","
      << "\"zoneId\":" << zone_id << ","
      << "\"positionMs\":" << position_ms << ","
      << "\"durationMs\":" << duration_ms << ","
      << "\"playing\":" << (playing ? "true" : "false")
      << '}';
  return out.str();
}

std::string SnapshotBuilder::build_cover_payload(const NowPlayingSnapshot& snapshot) {
  std::ostringstream out;
  out << "{"
      << "\"ownerZoneId\":" << snapshot.owner_zone_id << ","
      << "\"zoneId\":" << snapshot.owner_zone_id << ","
      << "\"trackId\":\"" << optional_json_string(snapshot.track_id) << "\","
      << "\"hasCoverArt\":" << (snapshot.has_cover_art ? "true" : "false") << ","
      << "\"coverArtId\":\"" << optional_json_string(snapshot.cover_art_id) << "\","
      << "\"coverArtUrl\":\"" << optional_json_string(cover_art_url(snapshot)) << "\""
      << '}';
  return out.str();
}

std::string SnapshotBuilder::build_history_payload(const std::vector<PlayHistoryEntry>& entries) {
  std::ostringstream out;
  out << "{\"limit\":" << entries.size() << ",\"entries\":[";
  for (std::size_t index = 0; index < entries.size(); ++index) {
    const auto& entry = entries[index];
    if (index > 0) {
      out << ',';
    }
    const std::string cover_url =
        entry.cover_art_id.empty()
            ? ""
            : "/api/audio/history/" + std::to_string(entry.id) + "/cover?v=sha256-" +
              entry.cover_art_id;
    out << "{"
        << "\"id\":" << entry.id << ","
        << "\"zoneId\":" << entry.zone_id << ","
        << "\"title\":\"" << optional_json_string(entry.title) << "\","
        << "\"artist\":\"" << optional_json_string(entry.artist) << "\","
        << "\"album\":\"" << optional_json_string(entry.album) << "\","
        << "\"trackId\":\"" << optional_json_string(entry.track_id) << "\","
        << "\"persistentId\":\"" << optional_json_string(entry.persistent_id) << "\","
        << "\"clientName\":\"" << optional_json_string(entry.client_name) << "\","
        << "\"clientModel\":\"" << optional_json_string(entry.client_model) << "\","
        << "\"durationMs\":" << entry.duration_ms << ","
        << "\"lastPositionMs\":" << entry.last_position_ms << ","
        << "\"hasCoverArt\":" << (entry.has_cover_art ? "true" : "false") << ","
        << "\"coverArtId\":\"" << optional_json_string(entry.cover_art_id) << "\","
        << "\"coverArtUrl\":\"" << optional_json_string(cover_url) << "\","
        << "\"startedAt\":\"" << optional_json_string(entry.started_at) << "\","
        << "\"endedAt\":\"" << optional_json_string(entry.ended_at) << "\","
        << "\"playedAt\":\"" << optional_json_string(entry.played_at) << "\""
        << '}';
  }
  out << "]}";
  return out.str();
}

std::string SnapshotBuilder::build_history_updated_payload(const PlayHistoryEntry& entry,
                                                           int limit) {
  const std::string cover_url =
      entry.cover_art_id.empty()
          ? ""
          : "/api/audio/history/" + std::to_string(entry.id) + "/cover?v=sha256-" +
            entry.cover_art_id;
  std::ostringstream out;
  out << "{"
      << "\"limit\":" << limit << ","
      << "\"latestHistoryId\":" << entry.id << ","
      << "\"latest\":{"
      << "\"zoneId\":" << entry.zone_id << ","
      << "\"title\":\"" << optional_json_string(entry.title) << "\","
      << "\"artist\":\"" << optional_json_string(entry.artist) << "\","
      << "\"album\":\"" << optional_json_string(entry.album) << "\","
      << "\"clientName\":\"" << optional_json_string(entry.client_name) << "\","
      << "\"hasCoverArt\":" << (entry.has_cover_art ? "true" : "false") << ","
      << "\"coverArtUrl\":\"" << optional_json_string(cover_url) << "\","
      << "\"startedAt\":\"" << optional_json_string(entry.started_at) << "\","
      << "\"endedAt\":\"" << optional_json_string(entry.ended_at) << "\""
      << "}}";
  return out.str();
}

}  // namespace homepi::metadata
