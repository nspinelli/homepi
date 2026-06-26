#pragma once

#include <chrono>

#include "homepi/audio-paging/types.hpp"

namespace homepi::audio_paging {

/** Controls DISABLED/COLD/WARM/ACTIVE state transitions and idle timers. */
class ResourceManager {
 public:
  /** Creates resource manager with default COLD state. */
  ResourceManager();

  /** Applies config toggles to state machine. */
  void apply_config(const PagingConfig& config);

  /** Marks job accepted and transitions to ACTIVE. */
  void on_job_started();

  /** Marks job completion and returns true when worker should remain warm. */
  bool on_job_finished(const PagingConfig& config);

  /** Returns true when idle timeout elapsed and worker should cool down. */
  bool should_cool_down(const PagingConfig& config) const;

  /** Forces immediate cold state. */
  void force_cold();

  /** Marks warm state after Piper successfully loaded. */
  void mark_warm();

  /** Returns current resource state. */
  ResourceState state() const;

 private:
  ResourceState state_ = ResourceState::Cold;
  std::chrono::steady_clock::time_point last_idle_at_{};
};

}  // namespace homepi::audio_paging
