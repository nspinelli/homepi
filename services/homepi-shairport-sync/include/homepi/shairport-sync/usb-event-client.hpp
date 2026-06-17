#pragma once

#include <atomic>
#include <functional>
#include <string>
#include <thread>

namespace homepi::shairport_sync {

/** Subscribes to a HomePi service Unix socket event stream. */
class UsbEventClient {
 public:
  UsbEventClient() = default;
  ~UsbEventClient();

  UsbEventClient(const UsbEventClient&) = delete;
  UsbEventClient& operator=(const UsbEventClient&) = delete;

  /**
   * Starts the background subscription loop.
   * @param socket_path - Unix socket path.
   * @param on_event - Callback invoked with each NDJSON line.
   */
  void start(const std::string& socket_path, std::function<void(const std::string&)> on_event);

  /** Stops the subscription loop. */
  void stop();

 private:
  void listen_loop();

  std::string socket_path_;
  std::function<void(const std::string&)> on_event_;
  std::thread thread_;
  std::atomic<bool> stop_{false};
};

}  // namespace homepi::shairport_sync
