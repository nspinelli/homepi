#include "homepi/audio-orchestrator/orchestrator.hpp"
#include "homepi/audio-orchestrator/service-config.hpp"
#include "homepi/audio-orchestrator/service-socket-client.hpp"

#include "homepi/events/events-client.hpp"

#include <atomic>
#include <chrono>
#include <csignal>
#include <iostream>
#include <thread>

namespace {

std::atomic<bool> g_stop{false};

void on_signal(int) { g_stop = true; }

}  // namespace

int main(int argc, char** argv) {
  const std::string config_path =
      argc > 1 ? argv[1] : "/opt/homepi/services/audio-orchestrator/config/service-config.json";

  const auto config = homepi::audio_orchestrator::load_service_config(config_path);

  homepi::audio_orchestrator::SocketPaths socket_paths{
      .pcm_router = config.pcm_router_socket,
      .hifi_serial = config.hifi_serial_socket,
      .nqptp_host = config.nqptp_host,
      .nqptp_port = config.nqptp_port,
  };

  homepi::audio_orchestrator::ServiceSocketClient socket_client(std::move(socket_paths));
  homepi::audio_orchestrator::Orchestrator orchestrator(config, std::move(socket_client));

  homepi::events::EventsClient events_client(config.events_socket, config.service);
  events_client.start(
      {"modules.shairport.session", "modules.shairport.volume", "modules.zone.config"},
      {"modules.audio.state"},
      [&orchestrator](const std::string& line) { orchestrator.handle_event_line(line); });

  std::signal(SIGINT, on_signal);
  std::signal(SIGTERM, on_signal);

  std::cout << config.service << " listening on " << config.events_socket << "\n";
  while (!g_stop.load()) {
    std::this_thread::sleep_for(std::chrono::milliseconds(200));
  }

  events_client.stop();
  return 0;
}
