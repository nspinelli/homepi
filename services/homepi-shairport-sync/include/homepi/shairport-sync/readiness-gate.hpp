#pragma once

#include "homepi/shairport-sync/types.hpp"

namespace homepi::shairport_sync {

class DbRepository;

/**
 * Evaluates whether all Shairport prerequisites are satisfied.
 */
class ReadinessGate {
 public:
  /**
   * Checks systemd units, ALSA loopback, and database state.
   * @param db Database repository.
   * @returns Readiness evaluation.
   */
  ReadinessResult evaluate(const DbRepository& db) const;
};

}  // namespace homepi::shairport_sync
