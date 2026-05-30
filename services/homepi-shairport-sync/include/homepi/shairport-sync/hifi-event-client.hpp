#pragma once

#include <atomic>
#include <functional>
#include <string>
#include <thread>

namespace homepi::shairport_sync {

/**
 * Subscribes to homepi-hifi-serial events over the Unix socket.
 */
class HifiEventClient {
 public:
  HifiEventClient() = default;
  ~HifiEventClient();

  HifiEventClient(const HifiEventClient&) = delete;
  HifiEventClient& operator=(const HifiEventClient&) = delete;

  /**
   * Starts background subscription thread.
   * @param socket_path HiFi serial socket path.
   * @param on_event Callback invoked for each event line.
   */
  void start(const std::string& socket_path,
             std::function<void(const std::string&)> on_event);

  /** Stops the subscription thread. */
  void stop();

  /**
   * Sends an RPC to the HiFi serial socket and returns the response line.
   * @param request NDJSON request payload.
   * @returns Response line or empty string on failure.
   */
  static std::string rpc(const std::string& socket_path, const std::string& request);

 private:
  void listen_loop();

  std::string socket_path_;
  std::function<void(const std::string&)> on_event_;
  std::atomic<bool> stop_{false};
  std::thread thread_;
};

}  // namespace homepi::shairport_sync
