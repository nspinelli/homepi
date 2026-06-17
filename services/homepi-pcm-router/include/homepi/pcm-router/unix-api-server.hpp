#pragma once

#include <atomic>
#include <functional>
#include <mutex>
#include <set>
#include <string>
#include <thread>

namespace homepi::pcm_router {

class RoutingState;
class AudioBridge;
class ActiveAudioConfig;

/** Socket command callback. */
using SocketCommandFn = std::function<void(const std::string& method, int zone_id,
                                           const std::string& correlation_id,
                                           const std::string& body_json)>;

/** Unix domain socket NDJSON server for pcm-router. */
class UnixApiServer {
 public:
  using SnapshotLineFn = std::function<std::string(const std::string& correlation_id)>;

  UnixApiServer(std::string socket_path, SnapshotLineFn snapshot_line_fn,
                SocketCommandFn on_command);
  ~UnixApiServer();

  bool start();
  void stop();
  void broadcast(const std::string& ndjson_line);

 private:
  void listen_loop();
  void handle_client(int client_fd);

  std::string socket_path_;
  SnapshotLineFn snapshot_line_fn_;
  SocketCommandFn on_command_;
  std::thread thread_;
  std::atomic<bool> stop_{false};
  int server_fd_ = -1;
  std::mutex clients_mutex_;
  std::set<int> subscribers_;
  std::set<int> active_clients_;
};

}  // namespace homepi::pcm_router
