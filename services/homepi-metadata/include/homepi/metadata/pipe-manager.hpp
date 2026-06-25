#pragma once

#include <atomic>
#include <functional>
#include <memory>
#include <mutex>
#include <string>
#include <unordered_set>
#include <vector>

#include "homepi/metadata/metadata-parser.hpp"
#include "homepi/metadata/zone-metadata-cache.hpp"

namespace homepi::metadata {

class ServiceEventLoop;

/** Callbacks invoked when owner-zone metadata is parsed from a FIFO. */
struct PipeManagerCallbacks {
  std::function<void(int zone_id, const std::string& field, const std::string& value)> on_field;
  std::function<void(int zone_id, int position_ms, int duration_ms, bool playing)> on_progress;
  std::function<void(int zone_id, bool playing)> on_playback_state;
  std::function<void(int zone_id, const std::vector<std::uint8_t>&)> on_cover_art;
  std::function<void(int zone_id)> on_metadata_bundle_start;
  std::function<void(int zone_id)> on_metadata_bundle_end;
  std::function<void(int zone_id)> on_session_cleared;
  std::function<void(int zone_id)> on_pipe_batch_complete;
};

/**
 * Manages enabled-zone metadata FIFOs with a single epoll loop.
 * Only the PCM owner pipe is parsed; other enabled pipes are drained and discarded.
 */
class PipeManager {
 public:
  PipeManager();
  ~PipeManager();

  PipeManager(const PipeManager&) = delete;
  PipeManager& operator=(const PipeManager&) = delete;

  /**
   * Prepares zone pipe state without starting a thread.
   * @param pipe_prefix FIFO path prefix ending before zone number.
   * @param zone_count Number of zones to monitor.
   * @param callbacks Parser callbacks for the active owner zone.
   */
  void prepare(const std::string& pipe_prefix, int zone_count, PipeManagerCallbacks callbacks);

  /**
   * Registers pipe fds with the unified service event loop (spec §15).
   * @param loop Shared epoll loop.
   */
  void attach(ServiceEventLoop& loop);

  /** Opens enabled-zone FIFOs that are not yet registered. */
  void open_missing_pipes();

  /** Stops pipe handling and closes FIFO handles. */
  void stop();

  /**
   * Updates which zone pipe is parsed into upstream callbacks.
   * @param owner_zone_id Active owner zone id or 0.
   */
  void set_owner_zone(int owner_zone_id);

  /**
   * Returns the active pipe owner zone id.
   * @returns Owner zone id or 0 when unassigned.
   */
  int owner_zone_id() const;

  /**
   * Replays cached pipe metadata into upstream callbacks for a zone.
   * @param zone_id Zone to synchronize.
   */
  void sync_zone_cache(int zone_id);

  /**
   * Restricts pipe opens to the given enabled zone ids.
   * @param enabled_zone_ids Enabled zone numbers.
   */
  void set_enabled_zones(const std::vector<int>& enabled_zone_ids);

 private:
  struct ZonePipe {
    int zone_id = 0;
    int fd = -1;
    std::shared_ptr<MetadataParser> parser;
  };

  struct PendingCoverArt {
    int zone_id = 0;
    std::vector<std::uint8_t> bytes;
  };

  void handle_readable(int fd);
  ZonePipe* find_zone_by_fd(int fd);
  void ensure_owner_parser(ZonePipe& zone);
  void reset_zone_parsers_locked();
  void replay_owner_cache(int zone_id);
  void dispatch_field(int zone_id, const std::string& field, const std::string& value);
  void dispatch_progress(int zone_id, const MetadataProgressUpdate& update);
  void dispatch_cover_art(int zone_id, const std::vector<std::uint8_t>& bytes);
  void flush_pending_cover_art();
  bool is_owner_zone(int zone_id) const;
  bool should_dispatch_to_service(int zone_id) const;
  void wake_loop();

  std::string pipe_prefix_;
  int zone_count_ = 0;
  PipeManagerCallbacks callbacks_;
  std::vector<ZonePipe> zones_;
  ZoneMetadataCacheStore cache_;
  std::atomic<bool> stop_{false};
  std::atomic<int> owner_zone_id_{0};
  std::unordered_set<int> enabled_zones_;
  int wake_fd_ = -1;
  ServiceEventLoop* loop_ = nullptr;
  std::mutex zones_mutex_;
  std::vector<PendingCoverArt> pending_cover_art_;
};

}  // namespace homepi::metadata
