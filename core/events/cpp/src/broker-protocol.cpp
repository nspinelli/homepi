#include "homepi/events/broker-protocol.hpp"

#include "homepi/events/event-envelope.hpp"

#include <chrono>
#include <random>
#include <sstream>

namespace homepi::events {

namespace {

std::string json_get_string(const std::string& json, const std::string& field) {
  const std::string needle = "\"" + field + "\":\"";
  const auto pos = json.find(needle);
  if (pos == std::string::npos) {
    return {};
  }
  const auto start = pos + needle.size();
  std::ostringstream out;
  for (std::size_t i = start; i < json.size(); ++i) {
    const char ch = json[i];
    if (ch == '"') {
      break;
    }
    if (ch == '\\' && i + 1 < json.size()) {
      const char next = json[i + 1];
      if (next == '"' || next == '\\' || next == 'n' || next == 'r' || next == 't') {
        out << next;
        ++i;
        continue;
      }
    }
    out << ch;
  }
  return out.str();
}

std::string json_get_object(const std::string& json, const std::string& field) {
  const std::string needle = "\"" + field + "\":";
  const auto pos = json.find(needle);
  if (pos == std::string::npos) {
    return {};
  }
  const auto start = json.find('{', pos);
  if (start == std::string::npos) {
    return {};
  }
  int depth = 0;
  for (std::size_t i = start; i < json.size(); ++i) {
    if (json[i] == '{') {
      ++depth;
    } else if (json[i] == '}') {
      --depth;
      if (depth == 0) {
        return json.substr(start, i - start + 1);
      }
    }
  }
  return {};
}

std::string create_request_id() {
  static thread_local std::mt19937_64 rng{
      static_cast<std::uint64_t>(
          std::chrono::steady_clock::now().time_since_epoch().count())};
  std::uniform_int_distribution<std::uint64_t> dist;
  std::ostringstream out;
  out << "req-" << dist(rng);
  return out.str();
}

std::string merge_event_payload(const std::string& event_name, const std::string& payload_json) {
  if (payload_json.empty() || payload_json == "{}") {
    return std::string("{\"event\":\"") + escape_json_string(event_name) + "\"}";
  }
  if (payload_json.size() >= 2 && payload_json.front() == '{' && payload_json.back() == '}') {
    return payload_json.substr(0, payload_json.size() - 1) + ",\"event\":\"" +
           escape_json_string(event_name) + "\"}";
  }
  return payload_json;
}

}  // namespace

bool is_v2_broker_socket(const std::string& socket_path) {
  return socket_path.find("/broker/") != std::string::npos ||
         socket_path.ends_with("broker.sock");
}

std::string build_broker_subscribe_line(const std::string& source,
                                        const std::vector<std::string>& topics) {
  std::ostringstream out;
  out << "{\"v\":1,\"id\":\"" << create_request_id() << "\",\"source\":\""
      << escape_json_string(source) << "\",\"target\":\"homepi-broker\",\"command\":\"subscribe\","
      << "\"correlationId\":\"events-subscribe\",\"payload\":{\"topics\":[";
  for (std::size_t i = 0; i < topics.size(); ++i) {
    if (i > 0) {
      out << ',';
    }
    out << '"' << escape_json_string(topics[i]) << '"';
  }
  out << "]}}";
  return out.str();
}

std::string build_broker_publish_line(const std::string& source,
                                      const std::string& legacy_envelope_line) {
  const std::string topic = json_get_string(legacy_envelope_line, "topic");
  const std::string envelope_source = json_get_string(legacy_envelope_line, "source");
  const std::string event_name = json_get_string(legacy_envelope_line, "event");
  const std::string correlation_id = json_get_string(legacy_envelope_line, "correlationId");
  const std::string payload_json = json_get_object(legacy_envelope_line, "payload");
  const std::string publish_source = envelope_source.empty() ? source : envelope_source;
  const std::string correlation =
      correlation_id.empty() ? create_request_id() : correlation_id;
  const std::string event_payload = merge_event_payload(event_name, payload_json);

  std::ostringstream out;
  out << "{\"v\":1,\"id\":\"" << create_request_id() << "\",\"source\":\""
      << escape_json_string(source) << "\",\"target\":\"homepi-broker\",\"command\":\"publish\","
      << "\"correlationId\":\"" << escape_json_string(correlation) << "\",\"payload\":{"
      << "\"topic\":\"" << escape_json_string(topic) << "\",\"source\":\""
      << escape_json_string(publish_source) << "\",\"severity\":\"info\",\"eventPayload\":"
      << event_payload << "}}";
  return out.str();
}

std::optional<std::string> broker_wire_to_legacy_envelope(const std::string& wire_line) {
  if (wire_line.find("\"type\":\"event\"") == std::string::npos) {
    return std::nullopt;
  }

  const std::string broker_event = json_get_object(wire_line, "event");
  if (broker_event.empty()) {
    return std::nullopt;
  }

  const std::string id = json_get_string(broker_event, "id");
  const std::string source = json_get_string(broker_event, "source");
  const std::string topic = json_get_string(broker_event, "topic");
  const std::string correlation_id = json_get_string(broker_event, "correlationId");
  const std::string timestamp = json_get_string(broker_event, "ts");
  const std::string payload_json = json_get_object(broker_event, "payload");
  const std::string event_name = json_get_string(payload_json, "event");

  if (topic.empty() || source.empty() || event_name.empty()) {
    return std::nullopt;
  }

  EventEnvelope envelope;
  envelope.version = 1;
  envelope.id = id.empty() ? create_request_id() : id;
  envelope.source = source;
  envelope.topic = topic;
  envelope.event = event_name;
  envelope.correlation_id = correlation_id.empty() ? envelope.id : correlation_id;
  envelope.timestamp = timestamp.empty() ? iso_timestamp() : timestamp;
  envelope.payload_json = payload_json.empty() ? "{}" : payload_json;
  return build_event_line(envelope);
}

}  // namespace homepi::events
