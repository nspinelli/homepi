#pragma once

#include <atomic>
#include <functional>
#include <mutex>
#include <string>
#include <thread>
#include <vector>

namespace homepi::metadata {

/** Callbacks retained for API compatibility; unused in drain-only mode. */
struct PipeManagerCallbacks {
  std::function<void(int zone_id, const std::string& field, const std::string& value)> on_field;
  std::function<void(int zone_id, int position_ms, int duration_ms, bool playing)> on_progress;
  std::function<void(int zone_id, bool playing)> on_playback_state;
  std::function<void(int zone_id, const std::vector<std::uint8_t>&)> on_cover_art;
  std::function<void(int zone_id)> on_metadata_bundle_start;
  std::function<void(int zone_id)> on_session_cleared;
};

/**
 * Manages all zone metadata FIFOs with a single epoll loop.
 * Drains every pipe so Shairport Sync never blocks; metadata is sourced from MQTT.
 */
class PipeManager {
 public:
  PipeManager();
  ~PipeManager();

  PipeManager(const PipeManager&) = delete;
  PipeManager& operator=(const PipeManager&) = delete;

  /**
   * Starts the epoll drain loop.
   * @param pipe_prefix FIFO path prefix ending before zone number.
   * @param zone_count Number of zones to monitor.
   * @param callbacks Unused (kept for service wiring stability).
   */
  void start(const std::string& pipe_prefix, int zone_count, PipeManagerCallbacks callbacks);

  /** Stops the epoll loop and closes FIFO handles. */
  void stop();

  /**
   * Retained for API compatibility with owner-zone changes.
   * @param owner_zone_id Active owner zone id or 0.
   */
  void set_owner_zone(int owner_zone_id);

 private:
  struct ZonePipe {
    int zone_id = 0;
    int fd = -1;
  };

  void run_loop();
  void open_missing_pipes();
  void handle_readable(int fd);
  void drain_zone(ZonePipe& zone);
  ZonePipe* find_zone_by_fd(int fd);

  std::string pipe_prefix_;
  int zone_count_ = 0;
  PipeManagerCallbacks callbacks_;
  std::vector<ZonePipe> zones_;
  std::thread thread_;
  std::atomic<bool> stop_{false};
  std::atomic<int> owner_zone_id_{0};
  int epoll_fd_ = -1;
  int wake_fd_ = -1;
  std::mutex zones_mutex_;
};

}  // namespace homepi::metadata
