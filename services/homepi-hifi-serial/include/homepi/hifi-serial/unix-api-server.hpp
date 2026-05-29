#pragma once

#include <atomic>
#include <functional>
#include <mutex>
#include <set>
#include <string>
#include <thread>

#include "homepi/hifi-serial/types.hpp"

namespace homepi::hifi_serial {

struct ApiContext {
  ServiceConfig config;
  std::function<ServiceHealth()> health_fn;
  std::function<std::string(const std::string& line)> handle_rpc_fn;
  std::function<std::string()> snapshot_json_fn;
};

/**
 * Unix socket server with RPC responses and event fan-out.
 */
class UnixApiServer {
 public:
  explicit UnixApiServer(ApiContext context);
  ~UnixApiServer();

  bool start();
  void stop();

  /**
   * Broadcasts an event line to all subscribers.
   * @param line NDJSON event envelope.
   */
  void broadcast(const std::string& line);

 private:
  void listen_loop();
  void handle_client(int client_fd);

  ApiContext context_;
  int server_fd_ = -1;
  std::atomic<bool> stop_{false};
  std::thread thread_;
  std::mutex clients_mutex_;
  std::set<int> subscribers_;
};

}  // namespace homepi::hifi_serial
