#pragma once

#include <atomic>
#include <functional>
#include <memory>
#include <mutex>
#include <string>
#include <vector>

namespace homepi::events {
class EventsClient;
}

namespace homepi::metadata {

/** Callback invoked when the PCM owner zone changes. */
using OwnerZoneChangeFn = std::function<void(int owner_zone_id)>;

/** Callback invoked when the enabled-zone mask changes. */
using EnabledZonesChangeFn = std::function<void(const std::vector<int>& enabled_zone_ids)>;

/** Callback invoked when PCM routing context changes without an owner promotion. */
using RoutingContextChangeFn = std::function<void(const std::string& payload_json,
                                                  const std::string& event_name)>;

/**
 * Tracks PCM owner and enabled zones from core/events broker messages.
 */
class EventsOwnerSubscriber {
 public:
  EventsOwnerSubscriber();
  ~EventsOwnerSubscriber();

  EventsOwnerSubscriber(const EventsOwnerSubscriber&) = delete;
  EventsOwnerSubscriber& operator=(const EventsOwnerSubscriber&) = delete;

  /**
   * Connects to the events broker and subscribes to PCM routing topics.
   * @param events_socket Broker socket path.
   * @param service Source name for registration.
   * @param on_owner_change Invoked when ownerZoneId changes.
   * @param on_enabled_zones_change Invoked when enabledZones changes.
   * @param on_routing_context_change Invoked for routing snapshots without owner promotion.
   */
  void start(const std::string& events_socket, const std::string& service,
             OwnerZoneChangeFn on_owner_change, EnabledZonesChangeFn on_enabled_zones_change,
             RoutingContextChangeFn on_routing_context_change = nullptr);

  /** Stops the broker client. */
  void stop();

  /**
   * Returns the last known owner zone id.
   * @returns Owner zone id or 0 when none.
   */
  int owner_zone_id() const;

  /**
   * Returns the last known enabled zone ids.
   * @returns Enabled zone numbers.
   */
  std::vector<int> enabled_zone_ids() const;

  /**
   * Returns the last known active stack zone ids.
   * @returns Active stack zone numbers.
   */
  std::vector<int> active_stack_zone_ids() const;

 private:
  void handle_event_line(const std::string& line);
  void apply_owner_zone(int owner_zone_id, bool authoritative);
  void apply_enabled_zones(const std::vector<int>& enabled_zone_ids);
  void apply_active_stack(const std::vector<int>& active_stack_zone_ids);

  std::unique_ptr<homepi::events::EventsClient> client_;
  OwnerZoneChangeFn on_owner_change_;
  EnabledZonesChangeFn on_enabled_zones_change_;
  RoutingContextChangeFn on_routing_context_change_;
  std::atomic<int> owner_zone_id_{0};
  mutable std::mutex enabled_mutex_;
  std::vector<int> enabled_zone_ids_;
  mutable std::mutex stack_mutex_;
  std::vector<int> active_stack_zone_ids_;
};

}  // namespace homepi::metadata
