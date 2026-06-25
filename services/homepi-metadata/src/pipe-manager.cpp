#include "homepi/metadata/pipe-manager.hpp"

#include <fcntl.h>
#include <sys/eventfd.h>
#include <sys/stat.h>
#include <unistd.h>

#include <cerrno>
#include <cstring>
#include <filesystem>
#include <string>

#include "homepi/metadata/metadata-normalizer.hpp"
#include "homepi/metadata/service-event-loop.hpp"

namespace fs = std::filesystem;

namespace homepi::metadata {

namespace {

int open_fifo(const std::string& path) {
  int fd = open(path.c_str(), O_RDONLY | O_NONBLOCK);
  if (fd < 0) {
    std::error_code ec;
    if (!fs::exists(path)) {
      if (mkfifo(path.c_str(), 0666) != 0) {
        return -1;
      }
      fd = open(path.c_str(), O_RDONLY | O_NONBLOCK);
    }
  }
  return fd;
}

}  // namespace

PipeManager::PipeManager() = default;

PipeManager::~PipeManager() { stop(); }

void PipeManager::prepare(const std::string& pipe_prefix, int zone_count,
                          PipeManagerCallbacks callbacks) {
  stop();
  pipe_prefix_ = pipe_prefix;
  zone_count_ = zone_count;
  callbacks_ = std::move(callbacks);
  stop_.store(false);
  wake_fd_ = eventfd(0, EFD_NONBLOCK | EFD_CLOEXEC);

  std::lock_guard lock(zones_mutex_);
  zones_.clear();
  zones_.reserve(static_cast<std::size_t>(zone_count_));
  enabled_zones_.clear();
  for (int zone = 1; zone <= zone_count_; ++zone) {
    ZonePipe entry;
    entry.zone_id = zone;
    zones_.push_back(std::move(entry));
    enabled_zones_.insert(zone);
  }
}

void PipeManager::attach(ServiceEventLoop& loop) {
  loop_ = &loop;
  if (wake_fd_ >= 0) {
    loop.add_readable(wake_fd_, [this](int fd) {
      uint64_t value = 0;
      read(fd, &value, sizeof(value));
      open_missing_pipes();
    });
  }
  open_missing_pipes();
}

void PipeManager::wake_loop() {
  if (wake_fd_ >= 0) {
    const uint64_t value = 1;
    write(wake_fd_, &value, sizeof(value));
  }
}

void PipeManager::stop() {
  stop_.store(true);
  wake_loop();
  std::lock_guard lock(zones_mutex_);
  if (loop_ != nullptr) {
    if (wake_fd_ >= 0) {
      loop_->remove_fd(wake_fd_);
    }
    for (auto& zone : zones_) {
      if (zone.fd >= 0) {
        loop_->remove_fd(zone.fd);
        close(zone.fd);
        zone.fd = -1;
      }
      zone.parser.reset();
    }
  } else {
    for (auto& zone : zones_) {
      if (zone.fd >= 0) {
        close(zone.fd);
        zone.fd = -1;
      }
      zone.parser.reset();
    }
  }
  if (wake_fd_ >= 0) {
    close(wake_fd_);
    wake_fd_ = -1;
  }
  loop_ = nullptr;
}

void PipeManager::set_enabled_zones(const std::vector<int>& enabled_zone_ids) {
  std::lock_guard lock(zones_mutex_);
  enabled_zones_.clear();
  for (const int zone_id : enabled_zone_ids) {
    if (zone_id > 0) {
      enabled_zones_.insert(zone_id);
    }
  }
  for (auto& zone : zones_) {
    if (enabled_zones_.contains(zone.zone_id)) {
      continue;
    }
    if (zone.fd >= 0) {
      if (loop_ != nullptr) {
        loop_->remove_fd(zone.fd);
      }
      close(zone.fd);
      zone.fd = -1;
    }
    zone.parser.reset();
    cache_.clear_zone(zone.zone_id);
  }
  wake_loop();
}

bool PipeManager::is_owner_zone(int zone_id) const {
  const int owner = owner_zone_id_.load();
  return owner > 0 && zone_id == owner;
}

bool PipeManager::should_dispatch_to_service(int zone_id) const {
  const int owner = owner_zone_id_.load();
  if (owner > 0) {
    return zone_id == owner;
  }
  if (enabled_zones_.empty()) {
    return zone_id > 0;
  }
  return enabled_zones_.contains(zone_id);
}

void PipeManager::reset_zone_parsers_locked() {
  for (auto& zone : zones_) {
    zone.parser.reset();
  }
}

void PipeManager::set_owner_zone(int owner_zone_id) {
  const int previous = owner_zone_id_.load();
  const bool owner_changed = previous != owner_zone_id;
  owner_zone_id_.store(owner_zone_id);
  if (owner_changed) {
    std::lock_guard lock(zones_mutex_);
    for (auto& zone : zones_) {
      if (owner_zone_id > 0 && zone.zone_id == owner_zone_id) {
        continue;
      }
      zone.parser.reset();
    }
  }
  if (owner_zone_id > 0) {
    replay_owner_cache(owner_zone_id);
  }
  wake_loop();
}

int PipeManager::owner_zone_id() const { return owner_zone_id_.load(); }

void PipeManager::sync_zone_cache(int zone_id) { replay_owner_cache(zone_id); }

void PipeManager::ensure_owner_parser(ZonePipe& zone) {
  if (zone.parser) {
    return;
  }

  const int owner = owner_zone_id_.load();
  if (owner > 0 && zone.zone_id != owner) {
    return;
  }
  if (owner <= 0 && !enabled_zones_.contains(zone.zone_id)) {
    return;
  }

  const int zone_id = zone.zone_id;
  MetadataParserCallbacks parser_callbacks;
  parser_callbacks.on_field = [this, zone_id](const MetadataFieldUpdate& update) {
    dispatch_field(zone_id, update.field, update.value);
  };
  parser_callbacks.on_progress = [this, zone_id](const MetadataProgressUpdate& update) {
    dispatch_progress(zone_id, update);
  };
  parser_callbacks.on_playback_state = [this, zone_id](bool playing) {
    ZoneMetadataCache& cache = cache_.zone(zone_id);
    cache.playing = playing;
    if (should_dispatch_to_service(zone_id) && callbacks_.on_playback_state) {
      callbacks_.on_playback_state(zone_id, playing);
    }
  };
  parser_callbacks.on_cover_art = [this, zone_id](const std::vector<std::uint8_t>& bytes) {
    dispatch_cover_art(zone_id, bytes);
  };
  parser_callbacks.on_metadata_bundle_start = [this, zone_id]() {
    cache_.begin_bundle(zone_id);
    if (should_dispatch_to_service(zone_id) && callbacks_.on_metadata_bundle_start) {
      callbacks_.on_metadata_bundle_start(zone_id);
    }
  };
  parser_callbacks.on_metadata_bundle_end = [this, zone_id]() {
    if (should_dispatch_to_service(zone_id) && callbacks_.on_metadata_bundle_end) {
      callbacks_.on_metadata_bundle_end(zone_id);
    }
  };
  parser_callbacks.on_session_cleared = [this, zone_id]() {
    cache_.clear_zone(zone_id);
    if (should_dispatch_to_service(zone_id) && callbacks_.on_session_cleared) {
      callbacks_.on_session_cleared(zone_id);
    }
  };

  zone.parser = std::make_shared<MetadataParser>(std::move(parser_callbacks));
}

void PipeManager::dispatch_field(int zone_id, const std::string& field,
                                 const std::string& value) {
  ZoneMetadataCache& cache = cache_.zone(zone_id);
  if (field == "title") {
    if (!value.empty()) {
      cache.title = value;
    }
  } else if (field == "artist") {
    cache.artist = value;
  } else if (field == "album") {
    if (!value.empty() && value != cache.title && value != cache.artist) {
      cache.album = value;
    }
  } else if (field == "client_name") {
    if (!value.empty() && !is_persistent_id_like(value)) {
      cache.client_name = value;
    }
  } else if (field == "client_model") {
    if (!value.empty() && !is_persistent_id_like(value)) {
      cache.client_model = value;
    }
  } else if (field == "track_id" || field == "persistent_id") {
    if (!value.empty() && value != cache.track_id && !cache.track_id.empty()) {
      cache.title.clear();
      cache.artist.clear();
      cache.album.clear();
      cache.has_cover_art = false;
      cache.duration_ms = 0;
    }
    if (!value.empty()) {
      cache.track_id = value;
    }
  } else if (field == "sort_title") {
    if (!value.empty()) {
      cache.sort_title = value;
    }
  }

  if (!should_dispatch_to_service(zone_id)) {
    return;
  }

  if (callbacks_.on_field) {
    callbacks_.on_field(zone_id, field, value);
  }
}

void PipeManager::dispatch_progress(int zone_id, const MetadataProgressUpdate& update) {
  ZoneMetadataCache& cache = cache_.zone(zone_id);
  if (update.has_position) {
    cache.position_ms = update.position_ms;
  }
  if (update.has_duration && update.duration_ms > 0) {
    cache.duration_ms = update.duration_ms;
  }
  if (update.playing) {
    cache.playing = true;
  }

  if (!should_dispatch_to_service(zone_id)) {
    return;
  }

  if (!callbacks_.on_progress) {
    return;
  }
  callbacks_.on_progress(
      zone_id, update.has_position ? update.position_ms : -1,
      update.has_duration ? update.duration_ms : -1, update.playing);
}

void PipeManager::dispatch_cover_art(int zone_id, const std::vector<std::uint8_t>& bytes) {
  if (bytes.empty()) {
    return;
  }
  cache_.zone(zone_id).has_cover_art = true;
  if (!should_dispatch_to_service(zone_id)) {
    return;
  }
  pending_cover_art_.push_back(PendingCoverArt{zone_id, bytes});
}

void PipeManager::flush_pending_cover_art() {
  if (pending_cover_art_.empty() || !callbacks_.on_cover_art) {
    pending_cover_art_.clear();
    return;
  }
  std::vector<PendingCoverArt> pending;
  pending.swap(pending_cover_art_);
  for (const PendingCoverArt& item : pending) {
    callbacks_.on_cover_art(item.zone_id, item.bytes);
  }
}

void PipeManager::replay_owner_cache(int zone_id) {
  if (!is_owner_zone(zone_id)) {
    return;
  }

  const ZoneMetadataCache* cache = cache_.find(zone_id);
  if (cache == nullptr) {
    return;
  }

  const auto replay_field = [&](const std::string& field, const std::string& value) {
    if (!value.empty() && callbacks_.on_field) {
      callbacks_.on_field(zone_id, field, value);
    }
  };

  replay_field("track_id", cache->track_id);
  replay_field("title", cache->title);
  if (cache->title.empty()) {
    replay_field("sort_title", cache->sort_title);
  }
  replay_field("artist", cache->artist);
  replay_field("album", cache->album);
  replay_field("client_name", cache->client_name);
  replay_field("client_model", cache->client_model);

  if (cache->duration_ms > 0 || cache->position_ms > 0) {
    if (callbacks_.on_progress) {
      callbacks_.on_progress(zone_id, cache->position_ms > 0 ? cache->position_ms : -1,
                             cache->duration_ms > 0 ? cache->duration_ms : -1, cache->playing);
    }
  }
}

void PipeManager::open_missing_pipes() {
  if (loop_ == nullptr) {
    return;
  }
  std::lock_guard lock(zones_mutex_);
  for (auto& zone : zones_) {
    if (!enabled_zones_.contains(zone.zone_id)) {
      if (zone.fd >= 0) {
        loop_->remove_fd(zone.fd);
        close(zone.fd);
        zone.fd = -1;
      }
      zone.parser.reset();
      continue;
    }
    if (zone.fd >= 0) {
      continue;
    }
    const std::string path = pipe_prefix_ + std::to_string(zone.zone_id);
    const int fd = open_fifo(path);
    if (fd < 0) {
      continue;
    }
    zone.fd = fd;
    loop_->add_readable(fd, [this, fd](int) { handle_readable(fd); });
  }
}

PipeManager::ZonePipe* PipeManager::find_zone_by_fd(int fd) {
  for (auto& zone : zones_) {
    if (zone.fd == fd) {
      return &zone;
    }
  }
  return nullptr;
}

void PipeManager::handle_readable(int fd) {
  std::string buffer;
  std::shared_ptr<MetadataParser> parser;
  {
    std::lock_guard lock(zones_mutex_);
    ZonePipe* zone = find_zone_by_fd(fd);
    if (zone == nullptr) {
      return;
    }

    const int owner = owner_zone_id_.load();
    const bool should_parse =
        owner > 0 ? is_owner_zone(zone->zone_id) : enabled_zones_.contains(zone->zone_id);
    if (should_parse) {
      ensure_owner_parser(*zone);
      parser = zone->parser;
    }

    while (true) {
      char chunk[65536];
      const ssize_t n = read(zone->fd, chunk, sizeof(chunk));
      if (n < 0) {
        if (errno == EAGAIN || errno == EWOULDBLOCK) {
          break;
        }
        if (loop_ != nullptr) {
          loop_->remove_fd(zone->fd);
        }
        close(zone->fd);
        zone->fd = -1;
        zone->parser.reset();
        parser.reset();
        break;
      }
      if (n == 0) {
        break;
      }
      if (should_parse) {
        buffer.append(chunk, static_cast<std::size_t>(n));
      }
    }
  }

  if (parser != nullptr && !buffer.empty()) {
    parser->feed(buffer.data(), buffer.size(), true);
    flush_pending_cover_art();
  }
}

}  // namespace homepi::metadata
