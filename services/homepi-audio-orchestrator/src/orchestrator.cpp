#include "homepi/audio-orchestrator/orchestrator.hpp"

#include "homepi/audio-orchestrator/airplay-source-loader.hpp"
#include "homepi/audio-orchestrator/json-utils.hpp"

#include <cmath>
#include <iostream>

namespace homepi::audio_orchestrator {

Orchestrator::Orchestrator(ServiceConfig config, ServiceSocketClient client)
    : config_(std::move(config)), client_(std::move(client)) {
  refresh_airplay_source();
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
  client_.send_hifi_command_async("*Z" + std::to_string(zone_id) + "POWER1");
  client_.send_hifi_command_async("*Z" + std::to_string(zone_id) + "SRC" +
                                  std::to_string(airplay_source()));
}

void Orchestrator::on_play_begin(int zone_id) {
  client_.pcm_route("route_start", zone_id);
  client_.nqptp_play_begin();
  client_.send_hifi_command_async("*Z" + std::to_string(zone_id) + "POWER1");
  client_.send_hifi_command_async("*Z" + std::to_string(zone_id) + "SRC" +
                                  std::to_string(airplay_source()));
}

void Orchestrator::on_play_end(int /*zone_id*/) {
  // Track boundaries must not tear down PCM routing.
}

void Orchestrator::on_active_end(int zone_id) {
  const std::string response = client_.pcm_route("route_end", zone_id);
  const int fallback_owner = client_.pcm_owner_from_response(response);
  if (fallback_owner > 0 && fallback_owner != zone_id) {
    client_.nqptp_play_begin();
    client_.send_hifi_command_async("*Z" + std::to_string(fallback_owner) + "POWER1");
    client_.send_hifi_command_async("*Z" + std::to_string(fallback_owner) + "SRC" +
                                    std::to_string(airplay_source()));
  }
  client_.send_hifi_command_async("*Z" + std::to_string(zone_id) + "POWER0");
}

void Orchestrator::on_volume_changed(int zone_id, const std::string& volume_db) {
  const int percent = volume_db_to_percent(volume_db);
  client_.send_hifi_command("*Z" + std::to_string(zone_id) + "VOLUME" + std::to_string(percent));
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
