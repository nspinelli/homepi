#include "homepi/events/broker-protocol.hpp"
#include "homepi/events/event-envelope.hpp"

#include "homepi/transport/unix-socket.hpp"

#include <sys/socket.h>
#include <syslog.h>
#include <unistd.h>

#include <cstdlib>
#include <cstring>
#include <iostream>
#include <regex>
#include <sstream>
#include <string>

namespace {

constexpr const char* kDefaultEventsSocket = "/run/homepi/broker/broker.sock";
constexpr const char* kDefaultAirplaySource = "5";
constexpr const char* kHookSource = "homepi-shairport-hook";

bool write_all(int fd, const std::string& data) {
  std::size_t offset = 0;
  while (offset < data.size()) {
    const ssize_t written = write(fd, data.data() + offset, data.size() - offset);
    if (written <= 0) {
      return false;
    }
    offset += static_cast<std::size_t>(written);
  }
  return true;
}

std::string env_or_default(const char* name, const char* fallback) {
  if (const char* value = std::getenv(name)) {
    if (*value != '\0') {
      return value;
    }
  }
  return fallback;
}

std::string map_action_to_event(const std::string& action) {
  if (action == "activate") {
    return "active_begin";
  }
  if (action == "deactivate") {
    return "active_end";
  }
  if (action == "play_begin" || action == "play_end" || action == "error") {
    return action;
  }
  if (action == "volume") {
    return "volume_changed";
  }
  return {};
}

std::string topic_for_event(const std::string& event) {
  if (event == "volume_changed") {
    return "modules.shairport.volume";
  }
  return "modules.shairport.session";
}

bool publish_event(const std::string& socket_path, const std::string& line) {
  const int fd = homepi::transport::connect_unix_stream_socket(socket_path);
  if (fd < 0) {
    return false;
  }

  if (homepi::events::is_v2_broker_socket(socket_path)) {
    const std::string command = homepi::events::build_broker_publish_line(kHookSource, line);
    const bool ok = write_all(fd, command + "\n");
    close(fd);
    return ok;
  }

  std::ostringstream register_line;
  register_line << "{\"method\":\"register\",\"source\":\"" << kHookSource
                << "\",\"subscribes\":[],\"publishes\":[\"modules.shairport.session\","
                << "\"modules.shairport.volume\"]}";

  const bool ok = write_all(fd, register_line.str() + "\n") && write_all(fd, line + "\n");
  close(fd);
  return ok;
}

}  // namespace

int main(int argc, char** argv) {
  if (argc < 3) {
    std::cerr << "usage: homepi-shairport-hook <action> <zone> [volume_db]\n";
    return 1;
  }

  const std::string action = argv[1];
  std::string zone_text = argv[2];
  std::string volume_db = argc > 3 ? argv[3] : "";

  if (volume_db.empty()) {
    static const std::regex zone_volume_pattern(R"(^([0-9]+)-([0-9.]+)$)");
    std::smatch match;
    if (std::regex_match(zone_text, match, zone_volume_pattern)) {
      zone_text = match[1].str();
      volume_db = "-" + match[2].str();
    }
  }

  const int zone_id = std::atoi(zone_text.c_str());
  if (zone_id <= 0) {
    std::cerr << "invalid zone: " << zone_text << "\n";
    return 1;
  }

  const std::string event = map_action_to_event(action);
  if (event.empty()) {
    std::cerr << "unknown action: " << action << "\n";
    return 1;
  }

  const std::string airplay_source = env_or_default("AIRPLAY_SOURCE", kDefaultAirplaySource);
  const std::string socket_path = env_or_default("HOMEPI_EVENTS_SOCKET", kDefaultEventsSocket);

  std::ostringstream payload;
  payload << "{\"zoneId\":" << zone_id << ",\"action\":\"" << homepi::events::escape_json_string(action)
          << "\",\"sourceNumber\":" << airplay_source;
  if (!volume_db.empty()) {
    payload << ",\"volumeDb\":\"" << homepi::events::escape_json_string(volume_db) << "\"";
  }
  payload << '}';

  homepi::events::EventEnvelope envelope;
  envelope.source = kHookSource;
  envelope.topic = topic_for_event(event);
  envelope.event = event;
  envelope.correlation_id = "shairport-hook-z" + std::to_string(zone_id) + "-" + action;
  envelope.timestamp = homepi::events::iso_timestamp();
  envelope.payload_json = payload.str();

  if (!publish_event(socket_path, homepi::events::build_event_line(envelope))) {
    if (action == "error") {
      openlog("homepi-shairport-error", LOG_PID, LOG_USER);
      syslog(LOG_ERR, "zone=%d unfixable error (broker publish failed)", zone_id);
      closelog();
    }
    std::cerr << "failed to publish " << event << " for zone " << zone_id << "\n";
    return 1;
  }

  if (action == "error") {
    openlog("homepi-shairport-error", LOG_PID, LOG_USER);
    syslog(LOG_ERR, "zone=%d unfixable error", zone_id);
    closelog();
  }

  return 0;
}
