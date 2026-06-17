#include "homepi/events/event-emitter.hpp"

#include <sstream>

#include "homepi/events/event-envelope.hpp"

namespace homepi::events {

EventEmitter::EventEmitter(std::string source, SinkFn sink)
    : source_(std::move(source)), sink_(std::move(sink)) {}

void EventEmitter::emit(const std::string& topic, const std::string& event,
                        const std::string& correlation_id, const std::string& payload_json) {
  if (!sink_) {
    return;
  }

  EventEnvelope envelope;
  envelope.id = source_ + "-" + std::to_string(++counter_);
  envelope.source = source_;
  envelope.topic = topic;
  envelope.event = event;
  envelope.correlation_id = correlation_id.empty() ? event : correlation_id;
  envelope.timestamp = iso_timestamp();
  envelope.payload_json = payload_json.empty() || payload_json.front() != '{' ? "{}" : payload_json;
  sink_(build_event_line(envelope));
}

void EventEmitter::emit_service_status(const std::string& event, const std::string& correlation_id,
                                       const std::string& status, const std::string& extra_json) {
  std::ostringstream payload;
  payload << "{\"status\":\"" << escape_json_string(status) << "\"";
  if (!extra_json.empty() && extra_json != "{}") {
    const std::string trimmed =
        extra_json.front() == '{' && extra_json.back() == '}'
            ? extra_json.substr(1, extra_json.size() - 2)
            : extra_json;
    if (!trimmed.empty()) {
      payload << ',' << trimmed;
    }
  }
  payload << '}';
  emit("system.service", event, correlation_id, payload.str());
}

}  // namespace homepi::events
