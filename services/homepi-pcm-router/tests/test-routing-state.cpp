#include <cassert>
#include <iostream>

#include "homepi/pcm-router/routing-state.hpp"

namespace {

void enable_all_zones(homepi::pcm_router::RoutingState& routing) {
  std::array<bool, homepi::pcm_router::kMaxZones + 1> mask{};
  mask.fill(true);
  routing.load_enabled_mask(mask);
}

}  // namespace

int main() {
  homepi::pcm_router::RoutingState routing;
  enable_all_zones(routing);
  routing.on_route_start(2);
  routing.on_route_join(5);
  assert(routing.owner_zone_id() == 2);
  const auto stack = routing.active_stack();
  assert(stack.size() == 2);
  assert(stack[0] == 2);
  const auto modes = routing.zone_modes();
  assert(modes[2] == homepi::pcm_router::ZoneCaptureMode::Buffer);
  assert(modes[5] == homepi::pcm_router::ZoneCaptureMode::Buffer);

  routing.on_route_end(2);
  assert(routing.owner_zone_id() == 5);
  const auto fallback_stack = routing.active_stack();
  assert(fallback_stack.size() == 1);
  assert(fallback_stack[0] == 5);

  // Simulates both zones firing route_end at a track boundary (play_end hook).
  // The second route_end empties the stack and drops the DAC — hooks must not
  // call route_end on play_end; only deactivate should tear down routing.
  {
    homepi::pcm_router::RoutingState multi_zone;
    enable_all_zones(multi_zone);
    multi_zone.on_route_start(1);
    multi_zone.on_route_join(2);
    multi_zone.on_route_end(1);
    assert(multi_zone.owner_zone_id() == 2);
    multi_zone.on_route_end(2);
    assert(multi_zone.owner_zone_id() == 0);
    assert(multi_zone.active_stack().empty());
  }

  routing.on_route_start(8);
  routing.on_route_start(3, [](int zone_id) { return zone_id == 8; }, 8);
  assert(routing.owner_zone_id() == 8);
  assert(routing.pending_owner_zone_id() == 3);
  const auto deferred_stack = routing.active_stack();
  assert(deferred_stack.size() == 2);
  assert(deferred_stack[0] == 8);
  assert(deferred_stack[1] == 3);
  assert(!routing.try_promote_pending_owner([](int /*zone_id*/) { return false; }));
  assert(routing.owner_zone_id() == 8);
  assert(routing.try_promote_pending_owner([](int /*zone_id*/) { return true; }));
  assert(routing.owner_zone_id() == 3);
  assert(routing.pending_owner_zone_id() == 0);
  const auto promoted_stack = routing.active_stack();
  assert(promoted_stack.size() == 2);
  assert(promoted_stack[0] == 3);
  assert(promoted_stack[1] == 8);

  {
    homepi::pcm_router::RoutingState disabled_routing;
    enable_all_zones(disabled_routing);
    disabled_routing.on_route_start(2);
    disabled_routing.on_route_join(5);
    disabled_routing.set_zone_enabled(5, false);
    assert(disabled_routing.active_stack().size() == 1);
    assert(disabled_routing.active_stack()[0] == 2);
    assert(disabled_routing.zone_modes()[5] ==
           homepi::pcm_router::ZoneCaptureMode::Disabled);
    disabled_routing.on_route_join(5);
    assert(disabled_routing.active_stack().size() == 1);

    const auto owner_disable = disabled_routing.set_zone_enabled(2, false);
    assert(owner_disable.owner_changed);
    assert(disabled_routing.owner_zone_id() == 0);
    assert(disabled_routing.active_stack().empty());
  }

  std::cout << "test_routing_state: OK\n";
  return 0;
}
