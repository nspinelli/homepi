#include <cassert>
#include <iostream>

#include "homepi/pcm-router/routing-state.hpp"

int main() {
  homepi::pcm_router::RoutingState routing;
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

  routing.on_route_start(8);
  routing.on_route_start(3, [](int zone_id) { return zone_id == 8; });
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

  routing.on_route_start(3);
  routing.on_route_start(8, [](int zone_id) { return zone_id == 8; });
  assert(routing.owner_zone_id() == 8);
  const auto solo_stack = routing.active_stack();
  assert(solo_stack.size() == 1);
  assert(solo_stack[0] == 8);

  routing.on_route_start(8);
  routing.on_route_start(3, [](int /*zone_id*/) { return false; });
  assert(routing.owner_zone_id() == 3);
  const auto join_stack = routing.active_stack();
  assert(join_stack.size() == 2);
  assert(join_stack[0] == 3);
  assert(join_stack[1] == 8);

  routing.on_route_start(3);
  routing.on_route_start(8, [](int /*zone_id*/) { return false; });
  assert(routing.owner_zone_id() == 8);
  const auto pruned_stack = routing.active_stack();
  assert(pruned_stack.size() == 1);
  assert(pruned_stack[0] == 8);

  std::cout << "test_routing_state: OK\n";
  return 0;
}
