#include "homepi/hifi-serial/event-publisher.hpp"

#include <chrono>
#include <ctime>
#include <sstream>

#include "homepi/hifi-serial/json-utils.hpp"

namespace homepi::hifi_serial {

namespace {

std::string iso_timestamp() {
  const auto now = std::chrono::system_clock::now();
  const std::time_t t = std::chrono::system_clock::to_time_t(now);
  std::tm tm{};
  gmtime_r(&t, &tm);
  char buffer[32];
  std::strftime(buffer, sizeof(buffer), "%Y-%m-%dT%H:%M:%S.000Z", &tm);
  return buffer;
}

}  // namespace

EventPublisher::EventPublisher(EventSink sink) : sink_(std::move(sink)) {}

std::string EventPublisher::next_id() const {
  return "evt-" + std::to_string(++counter_);
}

void EventPublisher::publish(const ParsedUpdate& update, const std::string& correlation_id) {
  if (!sink_) {
    return;
  }
  std::ostringstream out;
  out << "{"
      << "\"version\":1,"
      << "\"id\":\"" << next_id() << "\","
      << "\"source\":\"homepi-hifi-serial\","
      << "\"topic\":\"" << json_escape(update.topic) << "\","
      << "\"event\":\"" << json_escape(update.event_name) << "\","
      << "\"correlationId\":\"" << json_escape(correlation_id) << "\","
      << "\"timestamp\":\"" << iso_timestamp() << "\","
      << "\"payload\":" << update.payload_json
      << "}";
  sink_(out.str());
}

void EventPublisher::publish_snapshot(const std::string& snapshot_json,
                                      const std::string& correlation_id) {
  if (!sink_) {
    return;
  }
  std::ostringstream out;
  out << "{"
      << "\"version\":1,"
      << "\"id\":\"" << next_id() << "\","
      << "\"source\":\"homepi-hifi-serial\","
      << "\"topic\":\"modules.audio.snapshot\","
      << "\"event\":\"audio_state_snapshot\","
      << "\"correlationId\":\"" << json_escape(correlation_id) << "\","
      << "\"timestamp\":\"" << iso_timestamp() << "\","
      << "\"payload\":" << snapshot_json
      << "}";
  sink_(out.str());
}

}  // namespace homepi::hifi_serial
