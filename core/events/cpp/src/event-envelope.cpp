#include "homepi/events/event-envelope.hpp"

#include <chrono>
#include <ctime>
#include <iomanip>
#include <sstream>

namespace homepi::events {

namespace {

void append_escaped(std::ostringstream& out, const std::string& value) {
  for (char ch : value) {
    switch (ch) {
      case '"':
        out << "\\\"";
        break;
      case '\\':
        out << "\\\\";
        break;
      case '\n':
        out << "\\n";
        break;
      case '\r':
        out << "\\r";
        break;
      case '\t':
        out << "\\t";
        break;
      default:
        out << ch;
        break;
    }
  }
}

}  // namespace

std::string iso_timestamp() {
  const auto now = std::chrono::system_clock::now();
  const std::time_t t = std::chrono::system_clock::to_time_t(now);
  std::tm tm{};
  gmtime_r(&t, &tm);
  std::ostringstream out;
  out << std::put_time(&tm, "%Y-%m-%dT%H:%M:%S") << ".000Z";
  return out.str();
}

std::string escape_json_string(const std::string& value) {
  std::ostringstream out;
  append_escaped(out, value);
  return out.str();
}

std::string build_event_line(const EventEnvelope& envelope) {
  std::ostringstream out;
  out << '{'
      << "\"version\":" << envelope.version << ','
      << "\"id\":\"" << escape_json_string(envelope.id) << "\","
      << "\"source\":\"" << escape_json_string(envelope.source) << "\","
      << "\"topic\":\"" << escape_json_string(envelope.topic) << "\","
      << "\"event\":\"" << escape_json_string(envelope.event) << "\","
      << "\"correlationId\":\"" << escape_json_string(envelope.correlation_id) << "\","
      << "\"timestamp\":\"" << escape_json_string(envelope.timestamp) << "\","
      << "\"payload\":" << (envelope.payload_json.empty() ? "{}" : envelope.payload_json)
      << '}';
  return out.str();
}

}  // namespace homepi::events
