#pragma once

#include <atomic>
#include <cstdint>
#include <memory>
#include <mutex>
#include <string>
#include <thread>

namespace homepi::transport {
class LatestValuePublisher;
}

namespace homepi::metadata {

/**
 * Latest-value progress publisher on /run/homepi/audio-realtime.sock.
 */
class RealtimeProgressServer {
 public:
  RealtimeProgressServer();
  ~RealtimeProgressServer();

  RealtimeProgressServer(const RealtimeProgressServer&) = delete;
  RealtimeProgressServer& operator=(const RealtimeProgressServer&) = delete;

  /**
   * Binds the realtime socket and starts the epoll loop.
   * @param socket_path Absolute socket path.
   * @returns True when listening.
   */
  bool start(const std::string& socket_path);

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
  void listen_loop();
  void handle_client_line(int client_fd, const std::string& line);
  std::string build_frame(int owner_zone_id, const std::string& track_id, bool playing,
                          int position_ms, int duration_ms,
                          const std::string& progress_source);

  std::string socket_path_;
  std::unique_ptr<homepi::transport::LatestValuePublisher> publisher_;
  std::thread thread_;
  std::atomic<bool> stop_{false};
  int server_fd_ = -1;
  int epoll_fd_ = -1;
  std::atomic<std::uint64_t> sequence_{0};

  mutable std::mutex state_mutex_;
  int owner_zone_id_ = 0;
  std::string track_id_;
  bool playing_ = false;
  int position_ms_ = 0;
  int duration_ms_ = 0;
  std::string progress_source_ = "pipe:ssnc/prgr";
};

}  // namespace homepi::metadata
