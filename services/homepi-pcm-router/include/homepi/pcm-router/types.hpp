#pragma once

#include <cstdint>
#include <string>

namespace homepi::pcm_router {

/** Maximum supported AirPlay zones. */
constexpr int kMaxZones = 16;

/** Max wait before promoting a joining zone to DAC owner. */
constexpr int64_t kOwnerPromotionWaitMs = 0;

/** Live PCM must arrive within this window to count as active. */
constexpr int64_t kLivePcmActiveMs = 500;

/** Fresh frames for handoff promotion. */
constexpr int64_t kHandoffFreshBufferMs = 500;

/** Discard buffered PCM not refreshed within this window (stream ended). */
constexpr int64_t kCaptureStaleMs = 80;

/**
 * How long to keep draining a leaving zone's ring during handoff before treating
 * it as fully exhausted. Must be long enough to play out residual buffered PCM.
 */
constexpr int64_t kHandoffDrainMs = 1500;

/** Minimum buffered periods on the target zone before a seamless owner promote. */
constexpr int kHandoffMinPeriods = 2;

/** Zone capture behavior. */
enum class ZoneCaptureMode { Off, Drain, Buffer, Disabled };

/** DAC lifecycle for snapshots. */
enum class DacLifecycleState { Unassigned, Unavailable, Idle, Open, Paused };

/** Audio bridge runtime state. */
enum class AudioBridgeState { Stopped, Running, Paused, Degraded };

/** Runtime audio statistics. */
struct AudioBridgeStats {
  uint64_t capture_xruns = 0;
  uint64_t playback_xruns = 0;
  uint64_t frames_copied = 0;
};

/** Service configuration loaded from environment. */
struct ServiceConfig {
  std::string service = "homepi-pcm-router";
  std::string log_level = "INFO";
  std::string socket_path = "/run/homepi/audio/pcm-router.sock";
  std::string database_path = "/opt/homepi/runtime/state/homepi.sqlite";
  std::string artifact_path = "/opt/homepi/runtime/generated/audio/operating-profile.json";
  std::string usb_devices_socket = "/run/homepi/usb/usb.sock";
  std::string events_socket = "/run/homepi/broker/broker.sock";
  std::string loopback_card_a = "HomePiZonesA";
  std::string loopback_card_b = "HomePiZonesB";
  int zone_count = 16;
  uint32_t period_frames = 512;
  uint32_t buffer_frames = 4096;
  int64_t capture_idle_close_delay_ms = 5000;
};

}  // namespace homepi::pcm_router
