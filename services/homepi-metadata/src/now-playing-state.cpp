#include "homepi/metadata/now-playing-state.hpp"

#include "homepi/events/event-envelope.hpp"
#include "homepi/metadata/metadata-normalizer.hpp"

#include <cctype>

namespace homepi::metadata {

namespace {

bool is_client_field(const std::string& field) {
  return field == "client_name" || field == "client_model" || field == "client_user_agent" ||
         field == "client_ip" || field == "client_device_id" || field == "client_mac";
}

}  // namespace

bool NowPlayingState::set_owner_zone(int owner_zone_id) {
  std::lock_guard lock(mutex_);
  if (snapshot_.owner_zone_id == owner_zone_id) {
    return false;
  }
  const std::string preserved_client = snapshot_.client_name;
  const std::string preserved_model = snapshot_.client_model;
  const std::string preserved_user_agent = snapshot_.client_user_agent;
  const std::string preserved_ip = snapshot_.client_ip;
  const std::string preserved_device_id = snapshot_.client_device_id;
  const std::string preserved_mac = snapshot_.client_mac;
  snapshot_ = NowPlayingSnapshot{};
  snapshot_.owner_zone_id = owner_zone_id;
  snapshot_.client_name = preserved_client;
  snapshot_.client_model = preserved_model;
  snapshot_.client_user_agent = preserved_user_agent;
  snapshot_.client_ip = preserved_ip;
  snapshot_.client_device_id = preserved_device_id;
  snapshot_.client_mac = preserved_mac;
  if (owner_zone_id > 0) {
    snapshot_.started_at = homepi::events::iso_timestamp();
  }
  refresh_metadata_quality_locked();
  return true;
}

bool NowPlayingState::claim_owner_zone(int owner_zone_id) {
  std::lock_guard lock(mutex_);
  if (owner_zone_id <= 0 || snapshot_.owner_zone_id > 0) {
    return false;
  }
  snapshot_.owner_zone_id = owner_zone_id;
  snapshot_.started_at = homepi::events::iso_timestamp();
  refresh_metadata_quality_locked();
  return true;
}

void NowPlayingState::touch_started_at() {
  std::lock_guard lock(mutex_);
  snapshot_.started_at = homepi::events::iso_timestamp();
  touch_metadata_timestamp_locked();
}

bool NowPlayingState::apply_track_metadata(int zone_id, const std::string& track_id,
                                           const std::string& title, const std::string& artist,
                                           const std::string& album) {
  std::lock_guard lock(mutex_);
  if (zone_id <= 0 || zone_id != snapshot_.owner_zone_id) {
    return false;
  }
  if (!track_id.empty() && !snapshot_.track_id.empty() && snapshot_.track_id != track_id) {
    return false;
  }

  bool changed = false;
  if (!track_id.empty() && snapshot_.track_id != track_id) {
    snapshot_.track_id = track_id;
    snapshot_.persistent_id = track_id;
    changed = true;
  }
  if (!title.empty() && snapshot_.title != title) {
    snapshot_.title = title;
    changed = true;
  }
  if (!artist.empty() && snapshot_.artist != artist) {
    snapshot_.artist = artist;
    changed = true;
  }
  if (!album.empty() && !album.empty() && album != snapshot_.title && album != snapshot_.artist &&
      snapshot_.album != album) {
    snapshot_.album = album;
    changed = true;
  }
  if (!changed) {
    return false;
  }
  normalize_now_playing_snapshot(snapshot_);
  refresh_metadata_quality_locked();
  touch_metadata_timestamp_locked();
  return true;
}

bool NowPlayingState::update_field(int zone_id, const std::string& field,
                                   const std::string& value) {
  std::lock_guard lock(mutex_);
  if (zone_id <= 0 || zone_id != snapshot_.owner_zone_id) {
    return false;
  }

  const auto assign_string = [&](std::string& target) -> bool {
    if (value.empty() || target == value) {
      return false;
    }
    target = value;
    return true;
  };

  if (field == "title") {
    if (snapshot_.title == value) {
      return false;
    }
    snapshot_.title = value;
    normalize_now_playing_snapshot(snapshot_);
    refresh_metadata_quality_locked();
    touch_metadata_timestamp_locked();
    return true;
  }
  if (field == "artist") {
    if (!assign_string(snapshot_.artist)) {
      return false;
    }
    normalize_now_playing_snapshot(snapshot_);
    refresh_metadata_quality_locked();
    touch_metadata_timestamp_locked();
    return true;
  }
  if (field == "album") {
    if (value.empty() || value == snapshot_.title || value == snapshot_.artist ||
        snapshot_.album == value) {
      return false;
    }
    snapshot_.album = value;
    normalize_now_playing_snapshot(snapshot_);
    refresh_metadata_quality_locked();
    touch_metadata_timestamp_locked();
    return true;
  }
  if (field == "genre" && assign_string(snapshot_.genre)) {
    refresh_metadata_quality_locked();
    return true;
  }
  if (field == "composer" && assign_string(snapshot_.composer)) {
    refresh_metadata_quality_locked();
    return true;
  }
  if (field == "comment" && assign_string(snapshot_.comment)) {
    return true;
  }
  if (field == "sort_title" && assign_string(snapshot_.sort_title)) {
    if (snapshot_.title.empty() && !snapshot_.sort_title.empty()) {
      snapshot_.title = snapshot_.sort_title;
      normalize_now_playing_snapshot(snapshot_);
      refresh_metadata_quality_locked();
    }
    return true;
  }
  if (field == "file_kind" && assign_string(snapshot_.file_kind)) {
    return true;
  }
  if (field == "stream_url" && assign_string(snapshot_.stream_url)) {
    return true;
  }
  if (field == "client_name") {
    if (value.empty() || is_persistent_id_like(value) || value == snapshot_.track_id ||
        value == snapshot_.persistent_id) {
      return false;
    }
    if (snapshot_.client_name == value) {
      return false;
    }
    snapshot_.client_name = value;
    touch_metadata_timestamp_locked();
    return true;
  }
  if (field == "client_model" && assign_string(snapshot_.client_model)) {
    return true;
  }
  if (field == "client_user_agent" && assign_string(snapshot_.client_user_agent)) {
    return true;
  }
  if (field == "client_ip" && assign_string(snapshot_.client_ip)) {
    return true;
  }
  if (field == "client_device_id" && assign_string(snapshot_.client_device_id)) {
    return true;
  }
  if (field == "client_mac" && assign_string(snapshot_.client_mac)) {
    return true;
  }
  if (field == "track_id") {
    if (value.empty() || snapshot_.track_id == value) {
      return false;
    }
    if (!snapshot_.track_id.empty()) {
      snapshot_.title.clear();
      snapshot_.artist.clear();
      snapshot_.album.clear();
      snapshot_.has_cover_art = false;
      snapshot_.cover_art_id.clear();
      snapshot_.cover_art_path.clear();
      snapshot_.duration_ms = 0;
      snapshot_.position_ms = 0;
    }
    snapshot_.track_id = value;
    refresh_metadata_quality_locked();
    touch_metadata_timestamp_locked();
    return true;
  }
  if (field == "persistent_id") {
    if (value.empty() || snapshot_.persistent_id == value) {
      return false;
    }
    if (!snapshot_.persistent_id.empty() && snapshot_.persistent_id != value) {
      snapshot_.title.clear();
      snapshot_.artist.clear();
      snapshot_.album.clear();
      snapshot_.has_cover_art = false;
      snapshot_.cover_art_id.clear();
      snapshot_.cover_art_path.clear();
      snapshot_.duration_ms = 0;
      snapshot_.position_ms = 0;
      snapshot_.track_id.clear();
    }
    snapshot_.persistent_id = value;
    if (snapshot_.track_id.empty()) {
      snapshot_.track_id = value;
    }
    refresh_metadata_quality_locked();
    touch_metadata_timestamp_locked();
    return true;
  }
  if (field == "cover_art_id" && assign_string(snapshot_.cover_art_id)) {
    return true;
  }
  if (field == "cover_art_path" && assign_string(snapshot_.cover_art_path)) {
    return true;
  }

  (void)is_client_field;
  return false;
}

bool NowPlayingState::update_progress(int zone_id, int position_ms, int duration_ms,
                                      bool playing) {
  std::lock_guard lock(mutex_);
  if (zone_id <= 0 || zone_id != snapshot_.owner_zone_id) {
    return false;
  }
  bool changed = false;
  if (position_ms >= 0 && position_ms != snapshot_.position_ms) {
    snapshot_.position_ms = position_ms;
    changed = true;
  }
  if (duration_ms >= 0) {
    if (duration_ms == 0 && snapshot_.duration_ms > 0) {
      // Ignore zero-duration progress frames that would clobber a known track length.
    } else if (duration_ms != snapshot_.duration_ms) {
      snapshot_.duration_ms = duration_ms;
      changed = true;
    }
  }
  if (playing != snapshot_.playing) {
    snapshot_.playing = playing;
    changed = true;
  }
  if (!changed) {
    return false;
  }
  refresh_metadata_quality_locked();
  return true;
}

bool NowPlayingState::update_playing(int zone_id, bool playing) {
  std::lock_guard lock(mutex_);
  if (zone_id <= 0 || zone_id != snapshot_.owner_zone_id) {
    return false;
  }
  if (snapshot_.playing == playing) {
    return false;
  }
  snapshot_.playing = playing;
  return true;
}

bool NowPlayingState::mark_cover_art(int zone_id, const std::string& cover_art_id,
                                     const std::string& cover_art_path, bool force) {
  std::lock_guard lock(mutex_);
  if (zone_id <= 0 || zone_id != snapshot_.owner_zone_id) {
    return false;
  }
  if (snapshot_.has_cover_art && !force && snapshot_.cover_art_id == cover_art_id) {
    return false;
  }
  snapshot_.has_cover_art = true;
  if (!cover_art_id.empty()) {
    snapshot_.cover_art_id = cover_art_id;
  }
  if (!cover_art_path.empty()) {
    snapshot_.cover_art_path = cover_art_path;
  }
  refresh_metadata_quality_locked();
  touch_metadata_timestamp_locked();
  return true;
}

void NowPlayingState::clear_track_identity() {
  std::lock_guard lock(mutex_);
  snapshot_.title.clear();
  snapshot_.artist.clear();
  snapshot_.album.clear();
  snapshot_.genre.clear();
  snapshot_.composer.clear();
  snapshot_.comment.clear();
  snapshot_.sort_title.clear();
  snapshot_.has_cover_art = false;
  snapshot_.cover_art_id.clear();
  snapshot_.cover_art_path.clear();
  snapshot_.duration_ms = 0;
  snapshot_.track_id.clear();
  snapshot_.persistent_id.clear();
  refresh_metadata_quality_locked();
  touch_metadata_timestamp_locked();
}

void NowPlayingState::begin_metadata_bundle() {
  // Shairport emits multiple mdst/mden sequences per track (text, artwork, etc.).
  // Do not clear visible fields here; session/track boundaries use pend/pbeg instead.
}

void NowPlayingState::prepare_title_only_change() {
  std::lock_guard lock(mutex_);
  snapshot_.duration_ms = 0;
  snapshot_.has_cover_art = false;
  snapshot_.cover_art_id.clear();
  snapshot_.cover_art_path.clear();
}

void NowPlayingState::clear_metadata_fields() {
  std::lock_guard lock(mutex_);
  snapshot_.title.clear();
  snapshot_.artist.clear();
  snapshot_.album.clear();
  snapshot_.has_cover_art = false;
  snapshot_.cover_art_id.clear();
  snapshot_.cover_art_path.clear();
  refresh_metadata_quality_locked();
}

void NowPlayingState::clear_track_metadata() {
  std::lock_guard lock(mutex_);
  const int owner = snapshot_.owner_zone_id;
  snapshot_.title.clear();
  snapshot_.artist.clear();
  snapshot_.album.clear();
  snapshot_.has_cover_art = false;
  snapshot_.cover_art_id.clear();
  snapshot_.cover_art_path.clear();
  snapshot_.playing = false;
  snapshot_.position_ms = 0;
  snapshot_.duration_ms = 0;
  snapshot_.track_id.clear();
  snapshot_.persistent_id.clear();
  snapshot_.owner_zone_id = owner;
  refresh_metadata_quality_locked();
}

void NowPlayingState::clear() {
  std::lock_guard lock(mutex_);
  const int owner = snapshot_.owner_zone_id;
  snapshot_ = NowPlayingSnapshot{};
  snapshot_.owner_zone_id = owner;
  refresh_metadata_quality_locked();
}

void NowPlayingState::restore_updated_at(const std::string& updated_at) {
  std::lock_guard lock(mutex_);
  if (snapshot_.updated_at.empty() && !updated_at.empty()) {
    snapshot_.updated_at = updated_at;
  }
}

void NowPlayingState::touch_metadata_timestamp_locked() {
  snapshot_.updated_at = homepi::events::iso_timestamp();
}

void NowPlayingState::normalize_snapshot() {
  std::lock_guard lock(mutex_);
  normalize_now_playing_snapshot(snapshot_);
  refresh_metadata_quality_locked();
}

NowPlayingSnapshot NowPlayingState::snapshot() const {
  std::lock_guard lock(mutex_);
  return snapshot_;
}

NowPlayingSnapshot NowPlayingState::snapshot_for_finalize() const {
  std::lock_guard lock(mutex_);
  return snapshot_;
}

void NowPlayingState::apply_persisted_snapshot(const NowPlayingSnapshot& persisted) {
  std::lock_guard lock(mutex_);
  if (persisted.owner_zone_id <= 0) {
    return;
  }
  snapshot_.owner_zone_id = persisted.owner_zone_id;
  snapshot_.title = persisted.title;
  snapshot_.artist = persisted.artist;
  snapshot_.album = persisted.album;
  snapshot_.genre = persisted.genre;
  snapshot_.composer = persisted.composer;
  snapshot_.comment = persisted.comment;
  snapshot_.sort_title = persisted.sort_title;
  snapshot_.file_kind = persisted.file_kind;
  snapshot_.stream_url = persisted.stream_url;
  snapshot_.track_id = persisted.track_id;
  snapshot_.persistent_id = persisted.persistent_id;
  snapshot_.client_name = persisted.client_name;
  snapshot_.client_model = persisted.client_model;
  snapshot_.client_user_agent = persisted.client_user_agent;
  snapshot_.client_ip = persisted.client_ip;
  snapshot_.client_device_id = persisted.client_device_id;
  snapshot_.client_mac = persisted.client_mac;
  snapshot_.playing = persisted.playing;
  snapshot_.position_ms = persisted.position_ms;
  snapshot_.duration_ms = persisted.duration_ms;
  snapshot_.has_cover_art = persisted.has_cover_art;
  snapshot_.cover_art_id = persisted.cover_art_id;
  snapshot_.cover_art_path = persisted.cover_art_path;
  snapshot_.metadata_quality = persisted.metadata_quality;
  snapshot_.started_at = persisted.started_at;
  snapshot_.updated_at = persisted.updated_at;
  normalize_now_playing_snapshot(snapshot_);
  refresh_metadata_quality_locked();
}

void NowPlayingState::refresh_metadata_quality_locked() {
  if (snapshot_.title.empty() && snapshot_.artist.empty() && snapshot_.album.empty() &&
      snapshot_.track_id.empty() && snapshot_.persistent_id.empty() && !snapshot_.has_cover_art) {
    snapshot_.metadata_quality = "empty";
    return;
  }
  if (!snapshot_.title.empty() && !snapshot_.artist.empty() && snapshot_.duration_ms > 0) {
    snapshot_.metadata_quality = "complete";
    return;
  }
  snapshot_.metadata_quality = "partial";
}

}  // namespace homepi::metadata
