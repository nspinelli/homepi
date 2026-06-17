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
    for (int zone = 1; zone <= zone_count_; ++zone) {
      ZonePipe entry;
      entry.zone_id = zone;
      zones_.push_back(std::move(entry));
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
}

void PipeManager::set_owner_zone(int owner_zone_id) {
  owner_zone_id_.store(owner_zone_id);
  if (wake_fd_ >= 0) {
    const uint64_t value = 1;
    write(wake_fd_, &value, sizeof(value));
  }
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
  drain_zone(*zone);
}

void PipeManager::drain_zone(ZonePipe& zone) {
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
  }
}

}  // namespace homepi::metadata
