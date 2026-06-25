#pragma once

#include <atomic>
#include <functional>
#include <string>
#include <thread>

namespace homepi::metadata {

/** Callback invoked when the PCM router owner zone changes. */
using OwnerZoneFn = std::function<void(int owner_zone_id)>;

/** Callback invoked when PCM routing context changes without an owner promotion. */
using RoutingContextChangeFn = std::function<void(const std::string& payload_json,
                                                  const std::string& event_name)>;

/** Subscribes to homepi-pcm-router NDJSON event stream for owner zone updates. */
class PcmRouterSubscriber {
 public:
  PcmRouterSubscriber() = default;
  ~PcmRouterSubscriber();

  PcmRouterSubscriber(const PcmRouterSubscriber&) = delete;
  PcmRouterSubscriber& operator=(const PcmRouterSubscriber&) = delete;

  /**
   * Starts the background subscription loop.
   * @param socket_path PCM router Unix socket path.
   * @param on_owner_change Owner zone callback.
   * @param on_routing_context_change Routing snapshot callback.
   */
  void start(const std::string& socket_path, OwnerZoneFn on_owner_change,
             RoutingContextChangeFn on_routing_context_change = nullptr);

  /** Stops the subscription loop. */
  void stop();

  /**
   * Returns the cached owner zone id.
   * @returns Owner zone id or 0 when none.
   */
  int owner_zone_id() const;

 private:
  void listen_loop();
  void handle_line(const std::string& line);

  std::string socket_path_;
  OwnerZoneFn on_owner_change_;
  RoutingContextChangeFn on_routing_context_change_;
  std::thread thread_;
  std::atomic<bool> stop_{false};
  std::atomic<int> connect_fd_{-1};
  std::atomic<int> owner_zone_id_{0};
};

}  // namespace homepi::metadata
