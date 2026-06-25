#pragma once

#include <atomic>
#include <chrono>
#include <cstdint>
#include <memory>
#include <mutex>
#include <string>
#include <unordered_map>

namespace homepi::transport {
class LatestValuePublisher;
}

namespace homepi::metadata {

class ServiceEventLoop;

/**
 * Latest-value progress publisher on /run/homepi/audio-realtime.sock (spec §13).
 */
class RealtimeProgressServer {
 public:
  RealtimeProgressServer();
  ~RealtimeProgressServer();

  RealtimeProgressServer(const RealtimeProgressServer&) = delete;
  RealtimeProgressServer& operator=(const RealtimeProgressServer&) = delete;

  /**
   * Binds the realtime socket without starting a thread.
   * @param socket_path Absolute socket path.
   * @returns True when listening.
   */
  bool prepare(const std::string& socket_path);

  /**
   * Registers listener and client fds with the unified event loop.
   * @param loop Shared epoll loop.
   */
  void attach(ServiceEventLoop& loop);

  /** Stops the server and removes the socket path. */
  void stop();

  /**
   * Publishes the latest progress frame to connected clients.
   * @param owner_zone_id PCM owner zone id.
   * @param track_id Persistent track id.
   * @param playing Whether playback is active.
   * @param position_ms Current position in milliseconds.
   * @param duration_ms Track duration in milliseconds.
   * @param progress_source Diagnostic source label.
   */
  void publish_progress(int owner_zone_id, const std::string& track_id, bool playing,
                        int position_ms, int duration_ms, const std::string& progress_source);

 private:
  void accept_clients();
  void handle_client_readable(int client_fd);
  void handle_client_writable(int client_fd);
  void remove_client(int client_fd);
  void handle_client_line(int client_fd, const std::string& line);
  std::string build_frame(int owner_zone_id, const std::string& track_id, bool playing,
                          int position_ms, int duration_ms,
                          const std::string& progress_source);
  void send_initial_frame(int client_fd);

  std::string socket_path_;
  std::unique_ptr<homepi::transport::LatestValuePublisher> publisher_;
  std::atomic<bool> stop_{false};
  int server_fd_ = -1;
  ServiceEventLoop* loop_ = nullptr;
  std::atomic<std::uint64_t> sequence_{0};
  const std::chrono::steady_clock::time_point started_at_{std::chrono::steady_clock::now()};
  std::chrono::steady_clock::time_point last_publish_at_{};
  int publish_max_hz_ = 2;

  mutable std::mutex state_mutex_;
  int owner_zone_id_ = 0;
  std::string track_id_;
  bool playing_ = false;
  int position_ms_ = 0;
  int duration_ms_ = 0;
  std::string progress_source_ = "pipe:ssnc/prgr";
  std::unordered_map<int, std::string> client_buffers_;
};

}  // namespace homepi::metadata
