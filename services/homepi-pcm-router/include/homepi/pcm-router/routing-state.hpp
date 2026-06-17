#pragma once

#include <array>
#include <functional>
#include <mutex>
#include <vector>

#include "homepi/pcm-router/types.hpp"

namespace homepi::pcm_router {

/** Returns true when the zone is actively producing buffered PCM. */
using ZoneActivityFn = std::function<bool(int zone_id)>;

/** In-memory routing owner and active stack. */
class RoutingState {
 public:
  void set_routing(int owner_zone_id, const std::vector<int>& active_stack);
  void on_route_start(int zone_id);
  void on_route_start(int zone_id, const ZoneActivityFn& is_zone_active);
  void on_route_end(int zone_id);
  void on_route_join(int zone_id);

  void mark_zone_routed(int zone_id);
  bool zone_recently_routed(int zone_id, int64_t within_ms) const;

  /** Returns true when a deferred owner promotion was applied. */
  bool try_promote_pending_owner(const ZoneActivityFn& is_ready);

  int owner_zone_id() const;
  int pending_owner_zone_id() const;
  std::vector<int> active_stack() const;
  std::array<ZoneCaptureMode, kMaxZones + 1> zone_modes() const;

 private:
  void recompute_modes_locked();
  void push_stack_locked(int zone_id);
  void remove_from_stack_locked(int zone_id);

  mutable std::mutex mutex_;
  int owner_zone_id_ = 0;
  int pending_owner_zone_id_ = 0;
  int64_t pending_owner_at_ms_ = 0;
  std::vector<int> active_stack_;
  std::array<int64_t, kMaxZones + 1> last_route_at_ms_{};
  std::array<ZoneCaptureMode, kMaxZones + 1> zone_modes_{};
};

}  // namespace homepi::pcm_router
