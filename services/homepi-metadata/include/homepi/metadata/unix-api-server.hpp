#pragma once

#include <atomic>
#include <functional>
#include <mutex>
#include <set>
#include <string>
#include <thread>

namespace homepi::metadata {

using SnapshotLineFn = std::function<std::string(const std::string& correlation_id)>;

/**
 * Unix socket server that streams metadata event envelopes to subscribers.
 */
class UnixApiServer {
 public:
  UnixApiServer(std::string socket_path, SnapshotLineFn snapshot_line_fn);
  ~UnixApiServer();

  UnixApiServer(const UnixApiServer&) = delete;
  UnixApiServer& operator=(const UnixApiServer&) = delete;

  /**
   * Binds and starts the socket server.
   * @returns True on success.
   */
  bool start();

  /** Stops the socket server and removes the socket file. */
  void stop();

  /**
   * Broadcasts one NDJSON event line to all subscribers.
   * @param ndjson_line Event envelope without trailing newline.
   */
  void broadcast(const std::string& ndjson_line);

 private:
  void listen_loop();
  void handle_client(int client_fd);

  std::string socket_path_;
  SnapshotLineFn snapshot_line_fn_;
  int server_fd_ = -1;
  std::thread thread_;
  std::atomic<bool> stop_{false};
  std::mutex clients_mutex_;
  std::set<int> subscribers_;
  std::set<int> active_clients_;
};

}  // namespace homepi::metadata
