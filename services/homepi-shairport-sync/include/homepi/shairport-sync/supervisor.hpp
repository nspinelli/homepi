#pragma once

#include <atomic>
#include <condition_variable>
#include <map>
#include <mutex>
#include <string>

#include "homepi/shairport-sync/config-generator.hpp"
#include "homepi/shairport-sync/db-repository.hpp"
#include "homepi/shairport-sync/hifi-event-client.hpp"
#include "homepi/shairport-sync/readiness-gate.hpp"
#include "homepi/shairport-sync/systemd-controller.hpp"
#include "homepi/shairport-sync/types.hpp"
#include "homepi/shairport-sync/usb-event-client.hpp"

#include "homepi/storage/audio-profile-repository.hpp"
#include "homepi/storage/database-connection.hpp"

namespace homepi::shairport_sync {

/**
 * Orchestrates readiness gating, config generation, and zone lifecycle.
 */
class Supervisor {
 public:
  /**
   * @param config Service configuration.
   * @param db Database repository.
   */
  Supervisor(ServiceConfig config, DbRepository& db);

  /** Starts event subscription and main evaluation loop. */
  void run();

  /** Signals shutdown. */
  void stop();

  /** @returns Current health snapshot. */
  SupervisorHealth health() const;

  /**
   * Sets AirPlay source and triggers re-evaluation.
   * @param source_number Source slot 1-8.
   */
  void set_airplay_source(int source_number);

 private:
  void request_evaluate();
  void evaluate();
  void transition_offline(const std::vector<std::string>& failures);
  std::vector<int> enabled_zone_numbers() const;
  void reconcile_zone_units(const std::vector<int>& enabled);
  homepi::storage::AudioProfileTuple load_loopback_profile() const;

  ServiceConfig config_;
  DbRepository& db_;
  ReadinessGate gate_;
  ConfigGenerator generator_;
  SystemdController systemd_;
  HifiEventClient hifi_client_;
  UsbEventClient usb_client_;

  mutable std::mutex mutex_;
  std::condition_variable cv_;
  std::atomic<bool> running_{true};
  std::atomic<bool> evaluate_pending_{true};
  SupervisorState state_ = SupervisorState::offline;
  std::vector<std::string> failed_prerequisites_;
  std::map<int, std::string> config_hashes_;
  int active_zone_count_ = 0;
};

}  // namespace homepi::shairport_sync
