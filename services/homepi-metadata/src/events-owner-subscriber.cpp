#include "homepi/metadata/events-owner-subscriber.hpp"

#include <mutex>

#include "homepi/events/events-client.hpp"
#include "homepi/metadata/json-utils.hpp"

namespace homepi::metadata {

namespace {

std::vector<int> parse_enabled_zones_array(const std::string& payload) {
  std::vector<int> zones;
  const auto key_pos = payload.find("\"enabledZones\"");
  if (key_pos == std::string::npos) {
    return zones;
  }
  const auto open = payload.find('[', key_pos);
  const auto close = payload.find(']', open == std::string::npos ? key_pos : open);
  if (open == std::string::npos || close == std::string::npos || close <= open) {
    return zones;
  }
  std::string slice = payload.substr(open + 1, close - open - 1);
  std::size_t pos = 0;
  while (pos < slice.size()) {
    while (pos < slice.size() && (slice[pos] == ' ' || slice[pos] == ',')) {
      ++pos;
    }
    if (pos >= slice.size()) {
      break;
    }
    const std::size_t end = slice.find_first_of(",]", pos);
    const std::string token = slice.substr(pos, end == std::string::npos ? std::string::npos : end - pos);
    try {
      const int zone_id = std::stoi(token);
      if (zone_id > 0) {
        zones.push_back(zone_id);
      }
    } catch (...) {
    }
    if (end == std::string::npos) {
      break;
    }
    pos = end + 1;
  }
  return zones;
}

}  // namespace

EventsOwnerSubscriber::EventsOwnerSubscriber() = default;

EventsOwnerSubscriber::~EventsOwnerSubscriber() { stop(); }

void EventsOwnerSubscriber::start(const std::string& events_socket, const std::string& service,
                                  OwnerZoneChangeFn on_owner_change,
                                  EnabledZonesChangeFn on_enabled_zones_change) {
  stop();
  on_owner_change_ = std::move(on_owner_change);
  on_enabled_zones_change_ = std::move(on_enabled_zones_change);

  client_ = std::make_unique<homepi::events::EventsClient>(events_socket, service);
  client_->start(
      {"modules.pcm.snapshot", "modules.pcm.routing", "modules.pcm"},
      {"modules.metadata.snapshot", "modules.metadata.now_playing", "modules.metadata.cover_art"},
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

void EventsOwnerSubscriber::handle_event_line(const std::string& line) {
  if (line.find("\"topic\"") == std::string::npos) {
    return;
  }

  const std::string event = homepi::metadata::parse_event_name(line);
  if (event.empty()) {
    return;
  }

  const std::string payload = homepi::metadata::parse_payload_json(line);
  if (event == "pcm_router_snapshot" || event == "routing_changed" || event == "owner_changed") {
    const int owner = parse_int_field(payload, "ownerZoneId");
    apply_owner_zone(owner);
    const std::vector<int> enabled = parse_enabled_zones_array(payload);
    if (!enabled.empty()) {
      apply_enabled_zones(enabled);
    }
  }
}

void EventsOwnerSubscriber::apply_owner_zone(int owner_zone_id) {
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

}  // namespace homepi::metadata
