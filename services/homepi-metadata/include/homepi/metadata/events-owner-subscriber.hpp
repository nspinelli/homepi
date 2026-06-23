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
   */
  void start(const std::string& events_socket, const std::string& service,
             OwnerZoneChangeFn on_owner_change, EnabledZonesChangeFn on_enabled_zones_change);

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

 private:
  void handle_event_line(const std::string& line);
  void apply_owner_zone(int owner_zone_id);
  void apply_enabled_zones(const std::vector<int>& enabled_zone_ids);

  std::unique_ptr<homepi::events::EventsClient> client_;
  OwnerZoneChangeFn on_owner_change_;
  EnabledZonesChangeFn on_enabled_zones_change_;
  std::atomic<int> owner_zone_id_{0};
  mutable std::mutex enabled_mutex_;
  std::vector<int> enabled_zone_ids_;
};

}  // namespace homepi::metadata
