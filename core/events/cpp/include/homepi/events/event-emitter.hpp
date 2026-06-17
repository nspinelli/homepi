#pragma once

#include <functional>
#include <string>

namespace homepi::events {

/** Emits core event envelopes to a configured sink. */
class EventEmitter {
 public:
  using SinkFn = std::function<void(const std::string& ndjson_line)>;

  /**
   * Creates an event emitter for a service source name.
   * @param source Service source such as homepi-usb-devices.
   * @param sink Callback invoked with each NDJSON line.
   */
  EventEmitter(std::string source, SinkFn sink);

  /**
   * Emits a module event envelope.
   * @param topic Event topic.
   * @param event Event name.
   * @param correlation_id Correlation id.
   * @param payload_json Serialized JSON object payload.
   */
  void emit(const std::string& topic, const std::string& event, const std::string& correlation_id,
            const std::string& payload_json);

  /**
   * Emits a system.service health envelope.
   * @param event Event name.
   * @param correlation_id Correlation id.
   * @param status Health status string.
   * @param extra_json Additional JSON object fields without outer braces.
   */
  void emit_service_status(const std::string& event, const std::string& correlation_id,
                           const std::string& status, const std::string& extra_json = "{}");

 private:
  std::string source_;
  SinkFn sink_;
  unsigned long counter_ = 0;
};

}  // namespace homepi::events
