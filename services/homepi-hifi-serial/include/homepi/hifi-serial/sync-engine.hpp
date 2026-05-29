#pragma once

#include "homepi/hifi-serial/command-queue.hpp"

namespace homepi::hifi_serial {

/**
 * Runs efficient bulk sync queries on startup or manual trigger.
 */
class SyncEngine {
 public:
  /**
   * Enqueues all bulk protocol queries.
   * @param queue Command queue.
   */
  static void run_full_sync(CommandQueue& queue);
};

}  // namespace homepi::hifi_serial
