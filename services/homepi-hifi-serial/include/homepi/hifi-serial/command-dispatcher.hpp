#pragma once

#include <functional>
#include <string>
#include <vector>

#include "homepi/hifi-serial/command-queue.hpp"

namespace homepi::hifi_serial {

/** Callback invoked after typed commands are queued. */
using CommandStatusFn = std::function<void(const std::string& event, const std::string& correlation_id,
                                           int queued_count)>;

/**
 * Maps typed Hi-Fi command events to protocol encoders and enqueues serial commands.
 */
class CommandDispatcher {
 public:
  /**
   * Creates a dispatcher bound to the shared command queue.
   * @param queue Serial command queue.
   * @param on_status Optional callback for command_queued status events.
   */
  CommandDispatcher(CommandQueue& queue, CommandStatusFn on_status = nullptr);

  /**
   * Dispatches a typed command event.
   * @param event Typed event name such as set_zone_power_source.
   * @param payload_json JSON object text containing command fields.
   * @param correlation_id Request correlation id.
   * @returns Number of protocol commands queued.
   */
  int dispatch(const std::string& event, const std::string& payload_json,
               const std::string& correlation_id);

 private:
  void enqueue_commands(const std::vector<std::string>& commands, const std::string& event,
                        const std::string& correlation_id);

  CommandQueue& queue_;
  CommandStatusFn on_status_;
};

}  // namespace homepi::hifi_serial
