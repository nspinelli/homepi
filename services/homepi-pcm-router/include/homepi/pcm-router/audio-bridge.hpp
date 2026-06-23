#pragma once

#include <array>
#include <atomic>
#include <memory>
#include <mutex>
#include <string>
#include <thread>
#include <vector>

#include "homepi/pcm-router/active-audio-config.hpp"
#include "homepi/pcm-router/frame-ring.hpp"
#include "homepi/pcm-router/types.hpp"

struct _snd_pcm;

namespace homepi::pcm_router {

/** Zones closed during a capture lifecycle tick. */
struct CaptureLifecycleTick {
  std::vector<int> closed_zones;
};

/** Pass-through ALSA capture and DAC playback engine. */
class AudioBridge {
 public:
  struct StartResult {
    bool ok = false;
    std::string error;
    DacLifecycleState dac_state = DacLifecycleState::Unassigned;
  };

  explicit AudioBridge(const ServiceConfig& config);
  ~AudioBridge();

  AudioBridge(const AudioBridge&) = delete;
  AudioBridge& operator=(const AudioBridge&) = delete;

  StartResult start(const ActiveAudioConfig& config);
  bool reload(const ActiveAudioConfig& config);
  void pause();
  void stop();

  bool is_running() const;
  DacLifecycleState dac_state() const;
  AudioBridgeState bridge_state() const;
  AudioBridgeStats stats() const;
  ActiveAudioConfig active_config() const;

  void apply_zone_modes(const std::array<ZoneCaptureMode, kMaxZones + 1>& modes);
  void set_playback_owner(int zone_id);
  bool zone_recently_buffered(int zone_id, int64_t within_ms) const;
  size_t zone_available_frames(int zone_id) const;

  /** Opens capture in drain-only mode and cancels any pending idle close. */
  void prewarm_zone_capture(int zone_id);

  /** Closes capture immediately for a zone. */
  void disable_zone_capture(int zone_id);

  /** Schedules capture close after the configured idle grace period. */
  void schedule_zone_capture_idle_close(int zone_id);

  /**
   * Closes captures whose idle grace has elapsed.
   * @returns Zones that were closed during this tick.
   */
  CaptureLifecycleTick tick_capture_lifecycle();

  /** @returns Zone ids with an open capture worker. */
  std::vector<int> open_capture_zones() const;

  /** @returns Zone ids waiting for idle grace before capture close. */
  std::vector<int> closing_grace_capture_zones() const;

 private:
  struct ZoneCaptureSlot {
    std::atomic<bool> running{false};
    std::atomic<bool> stop_requested{false};
    std::atomic<int64_t> idle_close_at_ms{0};
    std::mutex mutex;
    _snd_pcm* handle = nullptr;
    std::thread thread;
  };

  void capture_loop(int zone_index);
  void playback_loop();
  bool prepare_playback_config(const ActiveAudioConfig& config);
  void close_all_captures();
  void close_playback_device();
  size_t bytes_per_frame() const;
  void request_dac_idle();
  void end_session_idle();
  bool ensure_dac_open_locked();
  void drop_and_close_dac_locked();
  void discard_stale_owner_frames(int owner, FrameRing& ring, uint8_t* buffer);
  bool playback_owner_unchanged(int owner, uint64_t revision) const;
  bool ensure_zone_capture_open(int zone_id);
  void close_zone_capture_internal(int zone_id);

  ServiceConfig config_;
  ActiveAudioConfig active_config_;
  std::array<std::atomic<ZoneCaptureMode>, kMaxZones + 1> zone_modes_{};
  std::array<ZoneCaptureSlot, kMaxZones + 1> capture_slots_{};
  std::atomic<int> playback_owner_{0};
  std::atomic<int64_t> owner_handoff_at_ms_{0};
  std::atomic<bool> dac_idle_requested_{false};
  std::array<std::atomic<int64_t>, kMaxZones + 1> last_buffered_at_ms_{};
  std::atomic<uint64_t> routing_revision_{0};
  std::atomic<bool> running_{false};
  std::atomic<bool> stop_requested_{false};
  std::atomic<DacLifecycleState> dac_state_{DacLifecycleState::Unassigned};
  std::atomic<AudioBridgeState> bridge_state_{AudioBridgeState::Stopped};
  AudioBridgeStats stats_{};

  std::mutex dac_mutex_;
  _snd_pcm* dac_handle_ = nullptr;
  std::vector<std::unique_ptr<FrameRing>> rings_;
  std::thread playback_thread_;
  size_t bytes_per_frame_ = 0;
};

}  // namespace homepi::pcm_router
