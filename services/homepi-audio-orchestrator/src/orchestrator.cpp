#include "homepi/audio-orchestrator/orchestrator.hpp"

#include "homepi/audio-orchestrator/airplay-source-loader.hpp"
#include "homepi/audio-orchestrator/json-utils.hpp"

#include "homepi/events/event-envelope.hpp"
#include "homepi/events/events-client.hpp"

#include <cmath>
#include <iostream>

namespace homepi::audio_orchestrator {

namespace {

std::string zone_power_source_payload(int zone_id, int source_number) {
  return "\"zoneNumber\":" + std::to_string(zone_id) + ",\"power\":true,\"sourceNumber\":" +
         std::to_string(source_number);
}

std::string zone_power_payload(int zone_id, bool power) {
  return "\"zoneNumber\":" + std::to_string(zone_id) + ",\"power\":" + (power ? "true" : "false");
}

std::string zone_volume_payload(int zone_id, int volume) {
  return "\"zoneNumber\":" + std::to_string(zone_id) + ",\"volume\":" + std::to_string(volume);
}

}  // namespace

Orchestrator::Orchestrator(ServiceConfig config, ServiceSocketClient client,
                           homepi::events::EventsClient* events_client)
    : config_(std::move(config)),
      client_(std::move(client)),
      events_client_(events_client) {
  refresh_airplay_source();
}

void Orchestrator::publish_hifi_command(const std::string& event,
                                        const std::string& payload_json,
                                        const std::string& correlation_id) const {
  if (events_client_ == nullptr) {
    client_.execute_hifi_command_async(event, payload_json);
    return;
  }

  homepi::events::EventEnvelope envelope;
  envelope.source = config_.service;
  envelope.topic = "modules.hifi.command";
  envelope.event = event;
  envelope.correlation_id = correlation_id;
  envelope.timestamp = homepi::events::iso_timestamp();
  envelope.id = config_.service + "-" + event + "-" + correlation_id;
  envelope.payload_json = "{" + payload_json + "}";
  events_client_->publish(homepi::events::build_event_line(envelope));
}

void Orchestrator::refresh_airplay_source() {
  AirplaySourceLoader loader(config_.database_path);
  if (const auto source = loader.load_from_database()) {
    airplay_source_ = *source;
    return;
  }
  airplay_source_ = config_.default_airplay_source;
}

void Orchestrator::handle_event_line(const std::string& line) {
  if (line.find("\"topic\"") == std::string::npos) {
    return;
  }

  const std::string topic_start = "\"topic\":\"";
  const auto topic_pos = line.find(topic_start);
  if (topic_pos == std::string::npos) {
    return;
  }
  const auto topic_value_start = topic_pos + topic_start.size();
  const auto topic_value_end = line.find('"', topic_value_start);
  if (topic_value_end == std::string::npos) {
    return;
  }
  const std::string topic = line.substr(topic_value_start, topic_value_end - topic_value_start);
  const std::string event = parse_event_name(line);
  const std::string payload = parse_payload_json(line);

  if (topic == "modules.shairport.session") {
    handle_session_event(event, payload);
    return;
  }
  if (topic == "modules.shairport.volume") {
    handle_volume_event(payload);
    return;
  }
  if (topic == "modules.zone.config") {
    handle_zone_config_event(event, payload);
  }
}

void Orchestrator::handle_session_event(const std::string& event,
                                        const std::string& payload_json) {
  const int zone_id = parse_int_field(payload_json, "zoneId");
  if (zone_id <= 0) {
    return;
  }

  if (event == "active_begin") {
    on_active_begin(zone_id);
  } else if (event == "play_begin") {
    on_play_begin(zone_id);
  } else if (event == "play_end") {
    on_play_end(zone_id);
  } else if (event == "active_end") {
    on_active_end(zone_id);
  }
}

void Orchestrator::handle_volume_event(const std::string& payload_json) {
  const int zone_id = parse_int_field(payload_json, "zoneId");
  const std::string volume_db = parse_string_field(payload_json, "volumeDb");
  if (zone_id <= 0 || volume_db.empty()) {
    return;
  }
  on_volume_changed(zone_id, volume_db);
}

void Orchestrator::handle_zone_config_event(const std::string& event,
                                            const std::string& payload_json) {
  if (event.find("airplay") != std::string::npos ||
      parse_int_field(payload_json, "sourceNumber") > 0) {
    refresh_airplay_source();
  }
}

void Orchestrator::on_active_begin(int zone_id) {
  client_.pcm_route("prewarm_capture", zone_id);
  publish_hifi_command("set_zone_power_source",
                       zone_power_source_payload(zone_id, airplay_source()),
                       "active-begin-z" + std::to_string(zone_id));
}

void Orchestrator::on_play_begin(int zone_id) {
  client_.pcm_route("route_start", zone_id);
  client_.nqptp_play_begin();
  publish_hifi_command("set_zone_power_source",
                       zone_power_source_payload(zone_id, airplay_source()),
                       "play-begin-z" + std::to_string(zone_id));
}

void Orchestrator::on_play_end(int /*zone_id*/) {
  // Track boundaries must not tear down PCM routing.
}

void Orchestrator::on_active_end(int zone_id) {
  const std::string correlation_id = "active-end-z" + std::to_string(zone_id);
  // Power off immediately so zones turn off even when PCM router is slow or unavailable.
  publish_hifi_command("set_zone_power", zone_power_payload(zone_id, false), correlation_id);

  const std::string response = client_.pcm_route("route_end", zone_id);
  const int fallback_owner = client_.pcm_owner_from_response(response);
  if (fallback_owner > 0 && fallback_owner != zone_id) {
    client_.nqptp_play_begin();
    publish_hifi_command("set_zone_power_source",
                         zone_power_source_payload(fallback_owner, airplay_source()),
                         correlation_id);
  }
}

void Orchestrator::on_volume_changed(int zone_id, const std::string& volume_db) {
  const int percent = volume_db_to_percent(volume_db);
  publish_hifi_command("set_zone_volume", zone_volume_payload(zone_id, percent),
                       "volume-z" + std::to_string(zone_id));
}

int Orchestrator::airplay_source() const { return airplay_source_; }

int Orchestrator::volume_db_to_percent(const std::string& volume_db) const {
  const double db = std::atof(volume_db.c_str());
  double percent = ((db + 30.0) / 30.0) * 100.0;
  if (percent < 0.0) {
    percent = 0.0;
  }
  if (percent > 100.0) {
    percent = 100.0;
  }
  return static_cast<int>(std::lround(percent));
}

}  // namespace homepi::audio_orchestrator
