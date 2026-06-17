#include "homepi/metadata/now-playing-state.hpp"

namespace homepi::metadata {

bool NowPlayingState::set_owner_zone(int owner_zone_id) {
  std::lock_guard lock(mutex_);
  if (snapshot_.owner_zone_id == owner_zone_id) {
    return false;
  }
  snapshot_ = NowPlayingSnapshot{};
  snapshot_.owner_zone_id = owner_zone_id;
  return true;
}

bool NowPlayingState::update_field(int zone_id, const std::string& field,
                                   const std::string& value) {
  std::lock_guard lock(mutex_);
  if (zone_id <= 0 || zone_id != snapshot_.owner_zone_id) {
    return false;
  }
  if (field == "title") {
    if (snapshot_.title == value) {
      return false;
    }
    snapshot_.title = value;
    return true;
  }
  if (field == "artist") {
    if (snapshot_.artist == value) {
      return false;
    }
    snapshot_.artist = value;
    return true;
  }
  if (field == "album") {
    if (snapshot_.album == value) {
      return false;
    }
    snapshot_.album = value;
    return true;
  }
  if (field == "client_name") {
    if (value.empty() || snapshot_.client_name == value) {
      return false;
    }
    // Prefer the longer name when MQTT delivers overlapping updates (e.g. parsed + raw).
    if (value.size() < snapshot_.client_name.size()) {
      return false;
    }
    snapshot_.client_name = value;
    return true;
  }
  return false;
}

bool NowPlayingState::update_progress(int zone_id, int position_ms, int duration_ms,
                                      bool playing) {
  std::lock_guard lock(mutex_);
  if (zone_id <= 0 || zone_id != snapshot_.owner_zone_id) {
    return false;
  }
  const bool next_position = position_ms >= 0 ? position_ms : snapshot_.position_ms;
  const bool next_duration = duration_ms >= 0 ? duration_ms : snapshot_.duration_ms;
  if (next_position == snapshot_.position_ms && next_duration == snapshot_.duration_ms &&
      playing == snapshot_.playing) {
    return false;
  }
  if (position_ms >= 0) {
    snapshot_.position_ms = position_ms;
  }
  if (duration_ms >= 0) {
    snapshot_.duration_ms = duration_ms;
  }
  snapshot_.playing = playing;
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

bool NowPlayingState::mark_cover_art(int zone_id, bool force) {
  std::lock_guard lock(mutex_);
  if (zone_id <= 0 || zone_id != snapshot_.owner_zone_id) {
    return false;
  }
  if (snapshot_.has_cover_art && !force) {
    return false;
  }
  snapshot_.has_cover_art = true;
  return true;
}

void NowPlayingState::clear_track_metadata() {
  std::lock_guard lock(mutex_);
  const int owner = snapshot_.owner_zone_id;
  snapshot_.title.clear();
  snapshot_.artist.clear();
  snapshot_.album.clear();
  snapshot_.playing = false;
  snapshot_.position_ms = 0;
  snapshot_.duration_ms = 0;
  snapshot_.has_cover_art = false;
  snapshot_.owner_zone_id = owner;
}

void NowPlayingState::clear() {
  std::lock_guard lock(mutex_);
  const int owner = snapshot_.owner_zone_id;
  snapshot_ = NowPlayingSnapshot{};
  snapshot_.owner_zone_id = owner;
}

NowPlayingSnapshot NowPlayingState::snapshot() const {
  std::lock_guard lock(mutex_);
  return snapshot_;
}

}  // namespace homepi::metadata
