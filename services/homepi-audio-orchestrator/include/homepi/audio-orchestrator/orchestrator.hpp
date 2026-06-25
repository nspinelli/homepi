#pragma once

#include <string>

#include "homepi/audio-orchestrator/service-config.hpp"
#include "homepi/audio-orchestrator/service-socket-client.hpp"

namespace homepi::events {
class EventsClient;
}

namespace homepi::audio_orchestrator {

/**
 * Applies Shairport session and volume events to PCM router and Hi-Fi serial services.
 */
class Orchestrator {
 public:
  /**
   * Creates an orchestrator with service configuration and socket client.
   * @param config Runtime configuration.
   * @param client Legacy socket RPC client.
   * @param events_client Broker client used to publish Hi-Fi commands.
   */
  Orchestrator(ServiceConfig config, ServiceSocketClient client,
               homepi::events::EventsClient* events_client);

  /**
   * Refreshes the AirPlay source number from SQLite or environment defaults.
   */
  void refresh_airplay_source();

  /**
   * Handles a broker event line.
   * @param line NDJSON envelope from core/events.
   */
  void handle_event_line(const std::string& line);

 private:
  void handle_session_event(const std::string& event, const std::string& payload_json);
  void handle_volume_event(const std::string& payload_json);
  void handle_zone_config_event(const std::string& event, const std::string& payload_json);

  void on_active_begin(int zone_id);
  void on_play_begin(int zone_id);
  void on_play_end(int zone_id);
  void on_active_end(int zone_id);
  void on_volume_changed(int zone_id, const std::string& volume_db);

  int airplay_source() const;
  int volume_db_to_percent(const std::string& volume_db) const;

  void publish_hifi_command(const std::string& event, const std::string& payload_json,
                            const std::string& correlation_id) const;

  ServiceConfig config_;
  ServiceSocketClient client_;
  homepi::events::EventsClient* events_client_ = nullptr;
  int airplay_source_ = 5;
};

}  // namespace homepi::audio_orchestrator
