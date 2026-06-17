#pragma once

#include <atomic>
#include <functional>
#include <string>
#include <thread>

namespace homepi::pcm_router {

/** Callback invoked for usb-devices profile events. */
using ProfileEventFn = std::function<void(const std::string& event, const std::string& payload_json)>;

/** Subscribes to homepi-usb-devices NDJSON event stream. */
class EventSubscriber {
 public:
  EventSubscriber() = default;
  ~EventSubscriber();

  EventSubscriber(const EventSubscriber&) = delete;
  EventSubscriber& operator=(const EventSubscriber&) = delete;

  /**
   * Starts the background subscription loop.
   * @param socket_path - usb-devices Unix socket path.
   * @param on_event - Profile event callback.
   */
  void start(const std::string& socket_path, ProfileEventFn on_event);

  /** Stops the subscription loop. */
  void stop();

 private:
  void listen_loop();

  std::string socket_path_;
  ProfileEventFn on_event_;
  std::thread thread_;
  std::atomic<bool> stop_{false};
  std::atomic<int> connect_fd_{-1};
};

}  // namespace homepi::pcm_router
