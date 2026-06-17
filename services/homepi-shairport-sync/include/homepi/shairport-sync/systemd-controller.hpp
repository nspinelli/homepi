#pragma once

#include <string>
#include <vector>

namespace homepi::shairport_sync {

/**
 * Controls systemd zone instances without auto-restart loops.
 * Metadata FIFO draining is handled by homepi-pcm-router.
 */
class SystemdController {
 public:
  /** Stops all homepi-shairport@ instances and legacy metadata@ instances. */
  void stop_all_zones(int zone_count) const;

  /**
   * Starts zone instances for enabled zones.
   * @param zone_numbers Zone ids to start.
   */
  void start_zones(const std::vector<int>& zone_numbers) const;

  /**
   * Restarts specific zone instances.
   * @param zone_numbers Zone ids to restart.
   */
  void restart_zones(const std::vector<int>& zone_numbers) const;

  /**
   * Stops zone and metadata instances for the given zone numbers.
   * @param zone_numbers Zone ids to stop.
   */
  void stop_zones(const std::vector<int>& zone_numbers) const;

  /**
   * @param unit Systemd unit name.
   * @returns True when unit is active.
   */
  bool is_unit_active(const std::string& unit) const;
};

}  // namespace homepi::shairport_sync
