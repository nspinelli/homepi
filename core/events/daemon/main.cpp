#include "homepi/events/broker.hpp"

#include <csignal>
#include <iostream>

namespace {

homepi::events::EventBroker* g_broker = nullptr;

void on_signal(int) {
  if (g_broker != nullptr) {
    g_broker->stop();
  }
}

}  // namespace

int main(int argc, char** argv) {
  std::string socket_path = "/run/homepi/events.sock";
  if (argc > 1) {
    socket_path = argv[1];
  }

  homepi::events::EventBroker broker(socket_path);
  g_broker = &broker;
  std::signal(SIGINT, on_signal);
  std::signal(SIGTERM, on_signal);

  std::cout << "homepi-events broker listening on " << socket_path << "\n";
  broker.run();
  return 0;
}
