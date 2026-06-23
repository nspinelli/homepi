#pragma once

#include <functional>
#include <string>

#include "homepi/hifi-serial/types.hpp"

namespace homepi::hifi_serial {

using EventSink = std::function<void(const std::string& event_line)>;

/**
 * Builds core event envelope JSON lines.
 */
class EventPublisher {
 public:
  explicit EventPublisher(EventSink sink);

  void publish(const ParsedUpdate& update, const std::string& correlation_id);

  void publish_snapshot(const std::string& snapshot_json, const std::string& correlation_id);

  /**
   * Publishes a typed command queued status event.
   * @param event Typed command event name.
   * @param correlation_id Request correlation id.
   * @param queued_count Number of protocol commands queued.
   */
  void publish_command_status(const std::string& event, const std::string& correlation_id,
                              int queued_count);

 private:
  std::string next_id() const;

  EventSink sink_;
  mutable int counter_ = 0;
};

}  // namespace homepi::hifi_serial
