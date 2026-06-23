#include "homepi/hifi-serial/events-command-subscriber.hpp"

#include "homepi/events/events-client.hpp"
#include "homepi/hifi-serial/json-utils.hpp"

namespace homepi::hifi_serial {

namespace {

std::string parse_event_name(const std::string& line) {
  return json_get_string(line, "event");
}

std::string parse_correlation_id(const std::string& line) {
  const std::string correlation_id = json_get_string(line, "correlationId");
  return correlation_id.empty() ? "broker" : correlation_id;
}

std::string parse_payload_json(const std::string& line) {
  const auto pos = line.find("\"payload\"");
  if (pos == std::string::npos) {
    return "{}";
  }
  const auto start = line.find('{', pos);
  if (start == std::string::npos) {
    return "{}";
  }
  int depth = 0;
  for (std::size_t i = start; i < line.size(); ++i) {
    if (line[i] == '{') {
      ++depth;
    } else if (line[i] == '}') {
      --depth;
      if (depth == 0) {
        return line.substr(start, i - start + 1);
      }
    }
  }
  return "{}";
}

}  // namespace

EventsCommandSubscriber::EventsCommandSubscriber(ServiceConfig config,
                                                 CommandDispatcher& dispatcher)
    : config_(std::move(config)), dispatcher_(dispatcher) {}

void EventsCommandSubscriber::start() {
  client_ = std::make_unique<homepi::events::EventsClient>(config_.events_socket, config_.service);
  client_->start(
      {"modules.hifi.command"},
      {"modules.hifi.command_status"},
      [this](const std::string& line) { handle_event_line(line); });
}

void EventsCommandSubscriber::stop() {
  if (client_) {
    client_->stop();
    client_.reset();
  }
}

void EventsCommandSubscriber::handle_event_line(const std::string& line) {
  if (line.find("\"topic\"") == std::string::npos) {
    return;
  }
  const std::string event = parse_event_name(line);
  if (event.empty()) {
    return;
  }
  dispatcher_.dispatch(event, parse_payload_json(line), parse_correlation_id(line));
}

}  // namespace homepi::hifi_serial
