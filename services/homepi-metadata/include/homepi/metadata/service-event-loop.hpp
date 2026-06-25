#pragma once

#include <chrono>
#include <cstdint>
#include <functional>
#include <unordered_map>
#include <vector>

namespace homepi::metadata {

/**
 * Unified epoll event loop for homepi-metadata (spec §15).
 * Watches pipe fds, realtime socket fds, timerfds, and shutdown eventfd.
 */
class ServiceEventLoop {
 public:
  /** Invoked when a registered fd is readable. */
  using ReadableHandler = std::function<void(int fd)>;

  /** Invoked when a one-shot or repeating timerfd fires. */
  using TimerHandler = std::function<void()>;

  /** Invoked when a registered fd is readable or writable. */
  using IoHandler = std::function<void(int fd, uint32_t events)>;

  ServiceEventLoop();
  ~ServiceEventLoop();

  ServiceEventLoop(const ServiceEventLoop&) = delete;
  ServiceEventLoop& operator=(const ServiceEventLoop&) = delete;

  /**
   * Creates epoll instance and shutdown eventfd.
   * @returns True when initialized.
   */
  bool init();

  /**
   * Registers a fd with the loop.
   * @param fd Non-blocking file descriptor.
   * @param events epoll event mask.
   * @param handler Callback invoked when events are ready.
   * @returns True when registered.
   */
  bool add_io_handler(int fd, uint32_t events, IoHandler handler);

  /**
   * Updates the epoll interest mask for a registered fd.
   * @param fd File descriptor.
   * @param events New epoll event mask.
   */
  void mod_io_events(int fd, uint32_t events);

  /**
   * Registers a readable fd with the loop.
   * @param fd Non-blocking file descriptor.
   * @param handler Callback invoked on EPOLLIN.
   * @returns True when registered.
   */
  bool add_readable(int fd, ReadableHandler handler);

  /**
   * Removes a fd from epoll.
   * @param fd File descriptor to remove.
   */
  void remove_fd(int fd);

  /**
   * Arms a one-shot metadata coalesce timer (spec §15.4).
   * @param delay_ms Debounce interval in milliseconds.
   * @param handler Callback when the timer fires.
   */
  void arm_metadata_flush_timer(int delay_ms, TimerHandler handler);

  /** Disarms the metadata coalesce timer. */
  void disarm_metadata_flush_timer();

  /**
   * Enables or disables the 5-second progress persist timer while playing.
   * @param enabled True when playback is active.
   * @param handler Callback when the timer fires.
   */
  void set_progress_persist_timer(bool enabled, TimerHandler handler);

  /**
   * Starts the maintenance timer (pipe reopen + artwork cleanup).
   * @param handler Callback when the timer fires.
   */
  void start_maintenance_timer(TimerHandler handler);

  /** Signals the loop to exit on the next epoll wake. */
  void request_shutdown();

  /**
   * Runs until should_stop returns true or shutdown is requested.
   * @param should_stop Poll predicate checked each iteration.
   */
  void run(const std::function<bool()>& should_stop);

  /**
   * Returns the epoll file descriptor for components that self-register.
   * @returns epoll fd or -1 when not initialized.
   */
  int epoll_fd() const;

 private:
  struct TimerSlot {
    int fd = -1;
    TimerHandler handler;
    bool repeating = false;
  };

  bool add_fd(int fd, uint32_t events);
  int create_timerfd(bool repeating, int interval_ms);
  void disarm_timer(TimerSlot& slot);
  void handle_timer(TimerSlot& slot);

  int epoll_fd_ = -1;
  int shutdown_fd_ = -1;
  TimerSlot metadata_flush_timer_{};
  TimerSlot progress_persist_timer_{};
  TimerSlot maintenance_timer_{};
  std::unordered_map<int, ReadableHandler> readable_handlers_;
  std::unordered_map<int, IoHandler> io_handlers_;
};

}  // namespace homepi::metadata
