#pragma once

#include <atomic>
#include <functional>
#include <memory>
#include <mutex>
#include <string>
#include <thread>
#include <unordered_set>
#include <vector>

#include "homepi/metadata/metadata-parser.hpp"

namespace homepi::metadata {

/** Callbacks invoked when owner-zone metadata is parsed from a FIFO. */
struct PipeManagerCallbacks {
  std::function<void(int zone_id, const std::string& field, const std::string& value)> on_field;
  std::function<void(int zone_id, int position_ms, int duration_ms, bool playing)> on_progress;
  std::function<void(int zone_id, bool playing)> on_playback_state;
  std::function<void(int zone_id, const std::vector<std::uint8_t>&)> on_cover_art;
  std::function<void(int zone_id)> on_metadata_bundle_start;
  std::function<void(int zone_id)> on_metadata_bundle_end;
  std::function<void(int zone_id)> on_session_cleared;
};

/**
 * Manages all zone metadata FIFOs with a single epoll loop.
 * Non-owner pipes are drained; the owner pipe is parsed for progress and duration.
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
   * @param callbacks Parser callbacks for the active owner zone.
   */
  void start(const std::string& pipe_prefix, int zone_count, PipeManagerCallbacks callbacks);

  /** Stops the epoll loop and closes FIFO handles. */
  void stop();

  /**
   * Updates which zone pipe is parsed instead of drained.
   * @param owner_zone_id Active owner zone id or 0.
   */
  void set_owner_zone(int owner_zone_id);

  /**
   * Restricts pipe opens to the given enabled zone ids.
   * @param enabled_zone_ids Enabled zone numbers.
   */
  void set_enabled_zones(const std::vector<int>& enabled_zone_ids);

 private:
  struct ZonePipe {
    int zone_id = 0;
    int fd = -1;
  };

  void run_loop();
  void open_missing_pipes();
  void handle_readable(int fd);
  void consume_zone(ZonePipe& zone);
  ZonePipe* find_zone_by_fd(int fd);
  void reset_owner_parser();

  std::string pipe_prefix_;
  int zone_count_ = 0;
  PipeManagerCallbacks callbacks_;
  std::vector<ZonePipe> zones_;
  std::thread thread_;
  std::atomic<bool> stop_{false};
  std::atomic<int> owner_zone_id_{0};
  std::unordered_set<int> enabled_zones_;
  int epoll_fd_ = -1;
  int wake_fd_ = -1;
  std::mutex zones_mutex_;
  std::unique_ptr<MetadataParser> owner_parser_;
};

}  // namespace homepi::metadata
