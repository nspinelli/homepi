#include "homepi/pcm-router/routing-state.hpp"

#include <algorithm>
#include <chrono>

namespace homepi::pcm_router {

namespace {

int64_t steady_now_ms() {
  return std::chrono::duration_cast<std::chrono::milliseconds>(
             std::chrono::steady_clock::now().time_since_epoch())
      .count();
}

}  // namespace

void RoutingState::mark_zone_routed(int zone_id) {
  if (zone_id < 1 || zone_id > kMaxZones) {
    return;
  }
  std::lock_guard lock(mutex_);
  last_route_at_ms_[zone_id] = steady_now_ms();
}

bool RoutingState::zone_recently_routed(int zone_id, int64_t within_ms) const {
  if (zone_id < 1 || zone_id > kMaxZones || within_ms <= 0) {
    return false;
  }
  std::lock_guard lock(mutex_);
  const int64_t last_route_at = last_route_at_ms_[zone_id];
  if (last_route_at <= 0) {
    return false;
  }
  return steady_now_ms() - last_route_at <= within_ms;
}

void RoutingState::push_stack_locked(int zone_id) {
  remove_from_stack_locked(zone_id);
  active_stack_.insert(active_stack_.begin(), zone_id);
}

void RoutingState::remove_from_stack_locked(int zone_id) {
  active_stack_.erase(std::remove(active_stack_.begin(), active_stack_.end(), zone_id),
                      active_stack_.end());
}

void RoutingState::prune_disabled_from_stack_locked() {
  active_stack_.erase(std::remove_if(active_stack_.begin(), active_stack_.end(),
                                     [this](int zone_id) {
                                       return zone_id < 1 || zone_id > kMaxZones ||
                                              !zone_enabled_[zone_id];
                                     }),
                      active_stack_.end());
  if (pending_owner_zone_id_ > 0 &&
      (pending_owner_zone_id_ > kMaxZones || !zone_enabled_[pending_owner_zone_id_])) {
    pending_owner_zone_id_ = 0;
    pending_owner_at_ms_ = 0;
  }
}

void RoutingState::recompute_modes_locked() {
  zone_modes_.fill(ZoneCaptureMode::Drain);
  for (int zone_id = 1; zone_id <= kMaxZones; ++zone_id) {
    if (!zone_enabled_[zone_id]) {
      zone_modes_[zone_id] = ZoneCaptureMode::Disabled;
    }
  }
  for (int zone_id : active_stack_) {
    if (zone_id >= 1 && zone_id <= kMaxZones && zone_enabled_[zone_id]) {
      zone_modes_[zone_id] = ZoneCaptureMode::Buffer;
    }
  }
  owner_zone_id_ = active_stack_.empty() ? 0 : active_stack_.front();
}

void RoutingState::load_enabled_mask(const std::array<bool, kMaxZones + 1>& mask) {
  std::lock_guard lock(mutex_);
  zone_enabled_ = mask;
  prune_disabled_from_stack_locked();
  recompute_modes_locked();
}

SetZoneEnabledResult RoutingState::set_zone_enabled(int zone_id, bool enabled) {
  SetZoneEnabledResult result;
  if (zone_id < 1 || zone_id > kMaxZones) {
    return result;
  }

  std::lock_guard lock(mutex_);
  if (zone_enabled_[zone_id] == enabled) {
    return result;
  }

  result.changed = true;
  result.zone_id = zone_id;
  result.enabled = enabled;
  zone_enabled_[zone_id] = enabled;

  if (!enabled) {
    const int previous_owner = owner_zone_id_;
    remove_from_stack_locked(zone_id);
    if (pending_owner_zone_id_ == zone_id) {
      pending_owner_zone_id_ = 0;
      pending_owner_at_ms_ = 0;
    }
    recompute_modes_locked();
    if (previous_owner == zone_id && owner_zone_id_ != previous_owner) {
      result.owner_changed = true;
      result.previous_owner_zone_id = previous_owner;
      result.new_owner_zone_id = owner_zone_id_;
    }
    return result;
  }

  recompute_modes_locked();
  return result;
}

bool RoutingState::is_zone_enabled(int zone_id) const {
  if (zone_id < 1 || zone_id > kMaxZones) {
    return false;
  }
  std::lock_guard lock(mutex_);
  return zone_enabled_[zone_id];
}

void RoutingState::set_routing(int owner_zone_id, const std::vector<int>& active_stack) {
  std::lock_guard lock(mutex_);
  active_stack_.clear();
  for (int zone_id : active_stack) {
    if (zone_id >= 1 && zone_id <= kMaxZones && zone_enabled_[zone_id]) {
      active_stack_.push_back(zone_id);
    }
  }
  if (owner_zone_id > 0 && zone_enabled_[owner_zone_id]) {
    push_stack_locked(owner_zone_id);
  }
  owner_zone_id_ = owner_zone_id > 0 && zone_enabled_[owner_zone_id]
                       ? owner_zone_id
                       : (active_stack_.empty() ? 0 : active_stack_.front());
  recompute_modes_locked();
}

void RoutingState::on_route_start(int zone_id) { on_route_start(zone_id, nullptr); }

void RoutingState::on_route_start(int zone_id, const ZoneActivityFn& is_zone_active) {
  if (zone_id < 1 || zone_id > kMaxZones) {
    return;
  }

  std::vector<int> stack_snapshot;
  int owner_snapshot = 0;
  std::array<int64_t, kMaxZones + 1> route_at_snapshot{};
  bool zone_enabled = false;
  {
    std::lock_guard lock(mutex_);
    if (!zone_enabled_[zone_id]) {
      return;
    }
    zone_enabled = true;
    stack_snapshot = active_stack_;
    owner_snapshot = owner_zone_id_;
    route_at_snapshot = last_route_at_ms_;
  }
  if (!zone_enabled) {
    return;
  }

  const auto is_session_active = [&](int stacked_zone_id) {
    if (stacked_zone_id >= 1 && stacked_zone_id <= kMaxZones) {
      const int64_t last_route_at = route_at_snapshot[stacked_zone_id];
      if (last_route_at > 0 && steady_now_ms() - last_route_at <= 5000) {
        return true;
      }
    }
    return is_zone_active ? is_zone_active(stacked_zone_id) : false;
  };

  std::lock_guard lock(mutex_);
  if (!zone_enabled_[zone_id]) {
    return;
  }

  if (is_zone_active) {
    if (owner_snapshot > 0 && owner_snapshot != zone_id && !is_session_active(owner_snapshot)) {
      active_stack_.clear();
      owner_zone_id_ = 0;
    } else {
      active_stack_.erase(std::remove_if(active_stack_.begin(), active_stack_.end(),
                                         [&](int stacked_zone_id) {
                                           if (stacked_zone_id == zone_id ||
                                               stacked_zone_id == owner_snapshot) {
                                             return false;
                                           }
                                           return !is_session_active(stacked_zone_id);
                                         }),
                          active_stack_.end());
    }
  }

  const int current_owner = active_stack_.empty() ? 0 : active_stack_.front();
  const bool defer_to_active_owner = is_zone_active && current_owner > 0 &&
                                     current_owner != zone_id &&
                                     is_session_active(current_owner);
  if (defer_to_active_owner) {
    if (std::find(active_stack_.begin(), active_stack_.end(), zone_id) == active_stack_.end()) {
      active_stack_.push_back(zone_id);
    }
    last_route_at_ms_[zone_id] = steady_now_ms();
    pending_owner_zone_id_ = zone_id;
    pending_owner_at_ms_ = steady_now_ms();
    recompute_modes_locked();
    return;
  }

  pending_owner_zone_id_ = 0;
  pending_owner_at_ms_ = 0;
  push_stack_locked(zone_id);
  last_route_at_ms_[zone_id] = steady_now_ms();
  recompute_modes_locked();
}

bool RoutingState::try_promote_pending_owner(const ZoneActivityFn& is_ready) {
  std::lock_guard lock(mutex_);
  if (pending_owner_zone_id_ <= 0 || pending_owner_zone_id_ > kMaxZones ||
      !zone_enabled_[pending_owner_zone_id_]) {
    return false;
  }
  const int pending_zone = pending_owner_zone_id_;
  const int64_t elapsed_ms = steady_now_ms() - pending_owner_at_ms_;
  const bool ready = is_ready && is_ready(pending_zone);
  if (!ready && elapsed_ms < kOwnerPromotionWaitMs) {
    return false;
  }
  push_stack_locked(pending_zone);
  pending_owner_zone_id_ = 0;
  pending_owner_at_ms_ = 0;
  recompute_modes_locked();
  return true;
}

void RoutingState::on_route_end(int zone_id) {
  if (zone_id < 1 || zone_id > kMaxZones) {
    return;
  }
  std::lock_guard lock(mutex_);
  if (!zone_enabled_[zone_id]) {
    return;
  }
  remove_from_stack_locked(zone_id);
  if (pending_owner_zone_id_ == zone_id || active_stack_.empty()) {
    pending_owner_zone_id_ = 0;
    pending_owner_at_ms_ = 0;
  }
  recompute_modes_locked();
}

void RoutingState::on_route_join(int zone_id) {
  if (zone_id < 1 || zone_id > kMaxZones) {
    return;
  }
  std::lock_guard lock(mutex_);
  if (!zone_enabled_[zone_id]) {
    return;
  }
  if (std::find(active_stack_.begin(), active_stack_.end(), zone_id) == active_stack_.end()) {
    active_stack_.push_back(zone_id);
  }
  recompute_modes_locked();
}

int RoutingState::owner_zone_id() const {
  std::lock_guard lock(mutex_);
  return owner_zone_id_;
}

int RoutingState::pending_owner_zone_id() const {
  std::lock_guard lock(mutex_);
  return pending_owner_zone_id_;
}

std::vector<int> RoutingState::active_stack() const {
  std::lock_guard lock(mutex_);
  return active_stack_;
}

std::vector<int> RoutingState::enabled_zones() const {
  std::lock_guard lock(mutex_);
  std::vector<int> zones;
  for (int zone_id = 1; zone_id <= kMaxZones; ++zone_id) {
    if (zone_enabled_[zone_id]) {
      zones.push_back(zone_id);
    }
  }
  return zones;
}

std::vector<int> RoutingState::disabled_zones() const {
  std::lock_guard lock(mutex_);
  std::vector<int> zones;
  for (int zone_id = 1; zone_id <= kMaxZones; ++zone_id) {
    if (!zone_enabled_[zone_id]) {
      zones.push_back(zone_id);
    }
  }
  return zones;
}

std::array<ZoneCaptureMode, kMaxZones + 1> RoutingState::zone_modes() const {
  std::lock_guard lock(mutex_);
  return zone_modes_;
}

}  // namespace homepi::pcm_router
