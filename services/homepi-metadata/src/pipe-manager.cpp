#include "homepi/metadata/pipe-manager.hpp"

#include <fcntl.h>
#include <sys/epoll.h>
#include <sys/eventfd.h>
#include <sys/stat.h>
#include <unistd.h>

#include <array>
#include <chrono>
#include <cerrno>
#include <cstring>
#include <filesystem>
#include <vector>

namespace homepi::metadata {

namespace {

constexpr int kEpollMaxEvents = 32;
constexpr int kPipeReopenSeconds = 5;

int open_fifo(const std::string& path) {
  int fd = open(path.c_str(), O_RDONLY | O_NONBLOCK);
  if (fd < 0) {
    std::error_code ec;
    if (!std::filesystem::exists(path)) {
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

void PipeManager::start(const std::string& pipe_prefix, int zone_count,
                        PipeManagerCallbacks callbacks) {
  stop();
  pipe_prefix_ = pipe_prefix;
  zone_count_ = zone_count;
  callbacks_ = std::move(callbacks);
  stop_.store(false);
  reset_owner_parser();

  epoll_fd_ = epoll_create1(EPOLL_CLOEXEC);
  wake_fd_ = eventfd(0, EFD_NONBLOCK | EFD_CLOEXEC);
  if (epoll_fd_ < 0 || wake_fd_ < 0) {
    stop();
    return;
  }

  epoll_event wake_event{};
  wake_event.events = EPOLLIN;
  wake_event.data.fd = wake_fd_;
  epoll_ctl(epoll_fd_, EPOLL_CTL_ADD, wake_fd_, &wake_event);

  {
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

  open_missing_pipes();
  thread_ = std::thread([this]() { run_loop(); });
}

void PipeManager::stop() {
  stop_.store(true);
  if (wake_fd_ >= 0) {
    const uint64_t value = 1;
    write(wake_fd_, &value, sizeof(value));
  }
  if (thread_.joinable()) {
    thread_.join();
  }
  if (epoll_fd_ >= 0) {
    close(epoll_fd_);
    epoll_fd_ = -1;
  }
  if (wake_fd_ >= 0) {
    close(wake_fd_);
    wake_fd_ = -1;
  }
  std::lock_guard lock(zones_mutex_);
  for (auto& zone : zones_) {
    if (zone.fd >= 0) {
      close(zone.fd);
      zone.fd = -1;
    }
  }
  owner_parser_.reset();
}

void PipeManager::set_owner_zone(int owner_zone_id) {
  owner_zone_id_.store(owner_zone_id);
  reset_owner_parser();
  if (wake_fd_ >= 0) {
    const uint64_t value = 1;
    write(wake_fd_, &value, sizeof(value));
  }
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
      epoll_ctl(epoll_fd_, EPOLL_CTL_DEL, zone.fd, nullptr);
      close(zone.fd);
      zone.fd = -1;
    }
  }
  if (wake_fd_ >= 0) {
    const uint64_t value = 1;
    write(wake_fd_, &value, sizeof(value));
  }
}

void PipeManager::reset_owner_parser() {
  const int owner_zone_id = owner_zone_id_.load();
  if (owner_zone_id <= 0) {
    owner_parser_.reset();
    return;
  }

  MetadataParserCallbacks parser_callbacks;
  parser_callbacks.on_field = [this, owner_zone_id](const MetadataFieldUpdate& update) {
    if (callbacks_.on_field) {
      callbacks_.on_field(owner_zone_id, update.field, update.value);
    }
  };
  parser_callbacks.on_progress = [this, owner_zone_id](const MetadataProgressUpdate& update) {
    if (!callbacks_.on_progress) {
      return;
    }
    callbacks_.on_progress(
        owner_zone_id,
        update.has_position ? update.position_ms : -1,
        update.has_duration ? update.duration_ms : -1,
        update.playing);
  };
  parser_callbacks.on_playback_state = [this, owner_zone_id](bool playing) {
    if (callbacks_.on_playback_state) {
      callbacks_.on_playback_state(owner_zone_id, playing);
    }
  };
  parser_callbacks.on_cover_art = [this, owner_zone_id](const std::vector<std::uint8_t>& bytes) {
    if (callbacks_.on_cover_art) {
      callbacks_.on_cover_art(owner_zone_id, bytes);
    }
  };
  parser_callbacks.on_metadata_bundle_start = [this, owner_zone_id]() {
    if (callbacks_.on_metadata_bundle_start) {
      callbacks_.on_metadata_bundle_start(owner_zone_id);
    }
  };
  parser_callbacks.on_metadata_bundle_end = [this, owner_zone_id]() {
    if (callbacks_.on_metadata_bundle_end) {
      callbacks_.on_metadata_bundle_end(owner_zone_id);
    }
  };
  parser_callbacks.on_session_cleared = [this, owner_zone_id]() {
    if (callbacks_.on_session_cleared) {
      callbacks_.on_session_cleared(owner_zone_id);
    }
  };

  owner_parser_ = std::make_unique<MetadataParser>(std::move(parser_callbacks));
}

void PipeManager::run_loop() {
  std::array<epoll_event, kEpollMaxEvents> events{};
  auto last_reopen = std::chrono::steady_clock::now();

  while (!stop_.load()) {
    const int ready = epoll_wait(epoll_fd_, events.data(), kEpollMaxEvents, 1000);
    if (ready < 0) {
      if (errno == EINTR) {
        continue;
      }
      break;
    }

    for (int i = 0; i < ready; ++i) {
      const int fd = events[i].data.fd;
      if (fd == wake_fd_) {
        uint64_t value = 0;
        read(wake_fd_, &value, sizeof(value));
        continue;
      }
      handle_readable(fd);
    }

    const auto now = std::chrono::steady_clock::now();
    if (now - last_reopen >= std::chrono::seconds(kPipeReopenSeconds)) {
      open_missing_pipes();
      last_reopen = now;
    }
  }
}

void PipeManager::open_missing_pipes() {
  std::lock_guard lock(zones_mutex_);
  for (auto& zone : zones_) {
    if (!enabled_zones_.contains(zone.zone_id)) {
      if (zone.fd >= 0) {
        epoll_ctl(epoll_fd_, EPOLL_CTL_DEL, zone.fd, nullptr);
        close(zone.fd);
        zone.fd = -1;
      }
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
    epoll_event event{};
    event.events = EPOLLIN;
    event.data.fd = fd;
    if (epoll_ctl(epoll_fd_, EPOLL_CTL_ADD, fd, &event) != 0) {
      close(fd);
      continue;
    }
    zone.fd = fd;
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
  std::lock_guard lock(zones_mutex_);
  ZonePipe* zone = find_zone_by_fd(fd);
  if (zone == nullptr) {
    return;
  }
  consume_zone(*zone);
}

void PipeManager::consume_zone(ZonePipe& zone) {
  const bool parse_owner = zone.zone_id == owner_zone_id_.load();
  char buffer[4096];
  while (true) {
    const ssize_t n = read(zone.fd, buffer, sizeof(buffer));
    if (n < 0) {
      if (errno == EAGAIN || errno == EWOULDBLOCK) {
        break;
      }
      epoll_ctl(epoll_fd_, EPOLL_CTL_DEL, zone.fd, nullptr);
      close(zone.fd);
      zone.fd = -1;
      break;
    }
    if (n == 0) {
      break;
    }
    if (parse_owner && owner_parser_ != nullptr) {
      owner_parser_->feed(buffer, static_cast<std::size_t>(n), true);
    }
  }
}

}  // namespace homepi::metadata
