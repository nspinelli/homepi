#pragma once

#include <memory>
#include <string>

#include "homepi/hifi-serial/command-dispatcher.hpp"
#include "homepi/hifi-serial/types.hpp"
#include "homepi/events/events-client.hpp"

namespace homepi::hifi_serial {

/**
 * Subscribes to modules.hifi.command broker events and dispatches typed serial commands.
 */
class EventsCommandSubscriber {
 public:
  /**
   * Creates a subscriber for the given service configuration.
   * @param config Service configuration.
   * @param dispatcher Typed command dispatcher.
   */
  EventsCommandSubscriber(ServiceConfig config, CommandDispatcher& dispatcher);

  /** Starts the broker client loop. */
  void start();

  /** Stops the broker client loop. */
  void stop();

 private:
  void handle_event_line(const std::string& line);

  ServiceConfig config_;
  CommandDispatcher& dispatcher_;
  std::unique_ptr<homepi::events::EventsClient> client_;
};

}  // namespace homepi::hifi_serial
