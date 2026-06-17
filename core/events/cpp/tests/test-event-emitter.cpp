#include <cassert>
#include <iostream>
#include <string>
#include <vector>

#include "homepi/events/event-emitter.hpp"

int main() {
  std::vector<std::string> lines;
  homepi::events::EventEmitter emitter("homepi-test", [&](const std::string& line) {
    lines.push_back(line);
  });

  emitter.emit("modules.test", "example_event", "corr-1", "{\"value\":1}");
  emitter.emit_service_status("service_ready", "startup", "healthy", "\"audioActive\":true");

  assert(lines.size() == 2);
  assert(lines[0].find("\"source\":\"homepi-test\"") != std::string::npos);
  assert(lines[0].find("\"event\":\"example_event\"") != std::string::npos);
  assert(lines[0].find("\"payload\":{\"value\":1}") != std::string::npos);
  assert(lines[1].find("\"topic\":\"system.service\"") != std::string::npos);
  assert(lines[1].find("\"status\":\"healthy\"") != std::string::npos);
  assert(lines[1].find("\"audioActive\":true") != std::string::npos);

  std::cout << "test_event_emitter: OK\n";
  return 0;
}
