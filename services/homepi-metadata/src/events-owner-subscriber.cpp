#include "homepi/metadata/events-owner-subscriber.hpp"

#include <algorithm>
#include <mutex>

#include "homepi/events/events-client.hpp"
#include "homepi/metadata/json-utils.hpp"

namespace homepi::metadata {

namespace {

std::vector<int> parse_enabled_zones_array(const std::string& payload) {
  return parse_int_array_field(payload, "enabledZones");
}

}  // namespace

EventsOwnerSubscriber::EventsOwnerSubscriber() = default;

EventsOwnerSubscriber::~EventsOwnerSubscriber() { stop(); }

void EventsOwnerSubscriber::start(const std::string& events_socket, const std::string& service,
                                  OwnerZoneChangeFn on_owner_change,
                                  EnabledZonesChangeFn on_enabled_zones_change,
                                  RoutingContextChangeFn on_routing_context_change) {
  stop();
  on_owner_change_ = std::move(on_owner_change);
  on_enabled_zones_change_ = std::move(on_enabled_zones_change);
  on_routing_context_change_ = std::move(on_routing_context_change);

  client_ = std::make_unique<homepi::events::EventsClient>(events_socket, service);
  client_->start(
      {"modules.pcm.routing", "modules.pcm.snapshot", "modules.zone.config", "core.service"},
      {"modules.metadata.snapshot", "modules.metadata.now_playing", "modules.metadata.cover_art",
       "modules.metadata.playback", "modules.metadata.history"},
      [this](const std::string& line) { handle_event_line(line); });
}

void EventsOwnerSubscriber::stop() {
  if (client_) {
    client_->stop();
    client_.reset();
  }
}

int EventsOwnerSubscriber::owner_zone_id() const { return owner_zone_id_.load(); }

std::vector<int> EventsOwnerSubscriber::enabled_zone_ids() const {
  std::lock_guard lock(enabled_mutex_);
  return enabled_zone_ids_;
}

std::vector<int> EventsOwnerSubscriber::active_stack_zone_ids() const {
  std::lock_guard lock(stack_mutex_);
  return active_stack_zone_ids_;
}

void EventsOwnerSubscriber::handle_event_line(const std::string& line) {
  if (line.find("\"topic\"") == std::string::npos) {
    return;
  }

  const std::string event = homepi::metadata::parse_event_name(line);
  if (event.empty()) {
    return;
  }

  const std::string payload = homepi::metadata::parse_payload_json(line);
  const std::vector<int> enabled = parse_enabled_zones_array(payload);
  if (!enabled.empty()) {
    apply_enabled_zones(enabled);
  }

  const std::vector<int> active_stack = parse_int_array_field(payload, "activeStack");
  if (!active_stack.empty()) {
    apply_active_stack(active_stack);
  }

  if (event == "owner_changed") {
    const int owner = parse_int_field(payload, "ownerZoneId");
    apply_owner_zone(owner, true);
    return;
  }

  if (event == "zone_enabled_changed") {
    const std::vector<int> enabled = parse_enabled_zones_array(payload);
    if (!enabled.empty()) {
      apply_enabled_zones(enabled);
    }
    return;
  }

  if (event == "owner_pending") {
    if (on_routing_context_change_) {
      on_routing_context_change_(payload, event);
    }
    return;
  }

  if (event == "pcm_router_snapshot" || event == "routing_changed") {
    if (on_routing_context_change_) {
      on_routing_context_change_(payload, event);
    }
    if (owner_zone_id_.load() <= 0) {
      const int owner = parse_int_field(payload, "ownerZoneId");
      const int bootstrap_owner = owner > 0 ? owner : (active_stack.empty() ? 0 : active_stack.front());
      if (bootstrap_owner > 0) {
        apply_owner_zone(bootstrap_owner, false);
      }
    }
  }
}

void EventsOwnerSubscriber::apply_owner_zone(int owner_zone_id, bool authoritative) {
  if (owner_zone_id == 0 && !authoritative) {
    return;
  }
  const int previous = owner_zone_id_.exchange(owner_zone_id);
  if (previous != owner_zone_id && on_owner_change_) {
    on_owner_change_(owner_zone_id);
  }
}

void EventsOwnerSubscriber::apply_enabled_zones(const std::vector<int>& enabled_zone_ids) {
  {
    std::lock_guard lock(enabled_mutex_);
    if (enabled_zone_ids_ == enabled_zone_ids) {
      return;
    }
    enabled_zone_ids_ = enabled_zone_ids;
  }
  if (on_enabled_zones_change_) {
    on_enabled_zones_change_(enabled_zone_ids);
  }
}

void EventsOwnerSubscriber::apply_active_stack(const std::vector<int>& active_stack_zone_ids) {
  {
    std::lock_guard lock(stack_mutex_);
    if (active_stack_zone_ids_ == active_stack_zone_ids) {
      return;
    }
    active_stack_zone_ids_ = active_stack_zone_ids;
  }
}

}  // namespace homepi::metadata
