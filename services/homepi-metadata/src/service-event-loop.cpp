#include "homepi/metadata/service-event-loop.hpp"

#include <sys/epoll.h>
#include <sys/eventfd.h>
#include <sys/timerfd.h>
#include <unistd.h>

#include <cerrno>
#include <cstring>

namespace homepi::metadata {

namespace {

constexpr int kMaxEvents = 64;

}  // namespace

ServiceEventLoop::ServiceEventLoop() = default;

ServiceEventLoop::~ServiceEventLoop() {
  disarm_metadata_flush_timer();
  disarm_timer(progress_persist_timer_);
  disarm_timer(maintenance_timer_);
  if (shutdown_fd_ >= 0) {
    remove_fd(shutdown_fd_);
    close(shutdown_fd_);
    shutdown_fd_ = -1;
  }
  if (epoll_fd_ >= 0) {
    close(epoll_fd_);
    epoll_fd_ = -1;
  }
}

bool ServiceEventLoop::init() {
  if (epoll_fd_ >= 0) {
    return true;
  }
  epoll_fd_ = epoll_create1(EPOLL_CLOEXEC);
  if (epoll_fd_ < 0) {
    return false;
  }
  shutdown_fd_ = eventfd(0, EFD_NONBLOCK | EFD_CLOEXEC);
  if (shutdown_fd_ < 0) {
    close(epoll_fd_);
    epoll_fd_ = -1;
    return false;
  }
  return add_readable(shutdown_fd_, [](int) {});
}

int ServiceEventLoop::epoll_fd() const { return epoll_fd_; }

bool ServiceEventLoop::add_fd(int fd, uint32_t events) {
  epoll_event ev{};
  ev.events = events;
  ev.data.fd = fd;
  return epoll_ctl(epoll_fd_, EPOLL_CTL_ADD, fd, &ev) == 0;
}

bool ServiceEventLoop::add_readable(int fd, ReadableHandler handler) {
  ReadableHandler stored = handler;
  if (!add_io_handler(fd, EPOLLIN, [stored](int ready_fd, uint32_t events) {
        if (events & EPOLLIN) {
          stored(ready_fd);
        }
      })) {
    return false;
  }
  readable_handlers_[fd] = std::move(handler);
  return true;
}

bool ServiceEventLoop::add_io_handler(int fd, uint32_t events, IoHandler handler) {
  if (fd < 0 || !handler) {
    return false;
  }
  if (!add_fd(fd, events)) {
    return false;
  }
  io_handlers_[fd] = std::move(handler);
  return true;
}

void ServiceEventLoop::mod_io_events(int fd, uint32_t events) {
  if (epoll_fd_ < 0 || fd < 0) {
    return;
  }
  epoll_event ev{};
  ev.events = events;
  ev.data.fd = fd;
  epoll_ctl(epoll_fd_, EPOLL_CTL_MOD, fd, &ev);
}

void ServiceEventLoop::remove_fd(int fd) {
  if (epoll_fd_ < 0 || fd < 0) {
    return;
  }
  epoll_ctl(epoll_fd_, EPOLL_CTL_DEL, fd, nullptr);
  readable_handlers_.erase(fd);
  io_handlers_.erase(fd);
}

int ServiceEventLoop::create_timerfd(bool repeating, int interval_ms) {
  const int fd = timerfd_create(CLOCK_MONOTONIC, TFD_NONBLOCK | TFD_CLOEXEC);
  if (fd < 0) {
    return -1;
  }
  const int ms = interval_ms > 0 ? interval_ms : 1;
  itimerspec spec{};
  spec.it_value.tv_sec = ms / 1000;
  spec.it_value.tv_nsec = static_cast<long>(ms % 1000) * 1'000'000L;
  if (repeating) {
    spec.it_interval = spec.it_value;
  }
  if (timerfd_settime(fd, 0, &spec, nullptr) != 0) {
    close(fd);
    return -1;
  }
  return fd;
}

void ServiceEventLoop::disarm_timer(TimerSlot& slot) {
  if (slot.fd < 0) {
    return;
  }
  remove_fd(slot.fd);
  close(slot.fd);
  slot.fd = -1;
  slot.handler = nullptr;
  slot.repeating = false;
}

void ServiceEventLoop::arm_metadata_flush_timer(int delay_ms, TimerHandler handler) {
  disarm_metadata_flush_timer();
  if (!handler || delay_ms < 0) {
    return;
  }
  metadata_flush_timer_.fd = create_timerfd(false, delay_ms);
  if (metadata_flush_timer_.fd < 0) {
    return;
  }
  metadata_flush_timer_.handler = std::move(handler);
  metadata_flush_timer_.repeating = false;
  add_readable(metadata_flush_timer_.fd, [this](int) { handle_timer(metadata_flush_timer_); });
}

void ServiceEventLoop::disarm_metadata_flush_timer() { disarm_timer(metadata_flush_timer_); }

void ServiceEventLoop::set_progress_persist_timer(bool enabled, TimerHandler handler) {
  disarm_timer(progress_persist_timer_);
  if (!enabled || !handler) {
    return;
  }
  progress_persist_timer_.fd = create_timerfd(true, 5000);
  if (progress_persist_timer_.fd < 0) {
    return;
  }
  progress_persist_timer_.handler = std::move(handler);
  progress_persist_timer_.repeating = true;
  add_readable(progress_persist_timer_.fd,
               [this](int) { handle_timer(progress_persist_timer_); });
}

void ServiceEventLoop::start_maintenance_timer(TimerHandler handler) {
  disarm_timer(maintenance_timer_);
  if (!handler) {
    return;
  }
  maintenance_timer_.fd = create_timerfd(true, 45'000);
  if (maintenance_timer_.fd < 0) {
    return;
  }
  maintenance_timer_.handler = std::move(handler);
  maintenance_timer_.repeating = true;
  add_readable(maintenance_timer_.fd, [this](int) { handle_timer(maintenance_timer_); });
}

void ServiceEventLoop::request_shutdown() {
  if (shutdown_fd_ < 0) {
    return;
  }
  const uint64_t value = 1;
  write(shutdown_fd_, &value, sizeof(value));
}

void ServiceEventLoop::handle_timer(TimerSlot& slot) {
  if (slot.fd < 0) {
    return;
  }
  uint64_t expirations = 0;
  read(slot.fd, &expirations, sizeof(expirations));
  if (slot.handler) {
    slot.handler();
  }
  if (!slot.repeating) {
    disarm_timer(slot);
  }
}

void ServiceEventLoop::run(const std::function<bool()>& should_stop) {
  epoll_event events[kMaxEvents];
  while (!should_stop()) {
    const int ready = epoll_wait(epoll_fd_, events, kMaxEvents, 1000);
    if (ready < 0) {
      if (errno == EINTR) {
        continue;
      }
      break;
    }
    for (int i = 0; i < ready; ++i) {
      const int fd = events[i].data.fd;
      if (fd == shutdown_fd_) {
        uint64_t value = 0;
        read(shutdown_fd_, &value, sizeof(value));
        return;
      }
      const auto io_it = io_handlers_.find(fd);
      if (io_it != io_handlers_.end() && io_it->second) {
        io_it->second(fd, events[i].events);
        continue;
      }
      const auto it = readable_handlers_.find(fd);
      if (it != readable_handlers_.end() && it->second) {
        it->second(fd);
      }
    }
  }
}

}  // namespace homepi::metadata
