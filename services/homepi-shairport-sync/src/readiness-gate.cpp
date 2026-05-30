#include "homepi/shairport-sync/readiness-gate.hpp"

#include "homepi/shairport-sync/db-repository.hpp"

#include <array>
#include <cstdio>
#include <memory>
#include <string>

namespace homepi::shairport_sync {

namespace {

std::string run_command(const char* command) {
  std::array<char, 256> buffer{};
  std::string output;
  const std::unique_ptr<FILE, decltype(&pclose)> pipe(popen(command, "r"), pclose);
  if (!pipe) {
    return output;
  }
  while (fgets(buffer.data(), static_cast<int>(buffer.size()), pipe.get()) != nullptr) {
    output += buffer.data();
  }
  while (!output.empty() && (output.back() == '\n' || output.back() == '\r')) {
    output.pop_back();
  }
  return output;
}

bool unit_active(const char* unit) {
  const std::string cmd = std::string("systemctl is-active ") + unit + " 2>/dev/null";
  return run_command(cmd.c_str()) == "active";
}

bool alsa_loopback_ready() {
  const std::string cards = run_command("aplay -l 2>/dev/null");
  return cards.find("HomePiZonesA") != std::string::npos &&
         cards.find("HomePiZonesB") != std::string::npos;
}

}  // namespace

ReadinessResult ReadinessGate::evaluate(const DbRepository& db) const {
  ReadinessResult result;

  if (!unit_active("homepi-nqptp.service")) {
    result.failures.push_back("homepi-nqptp not active");
  }
  if (!unit_active("homepi-pcm-router.service")) {
    result.failures.push_back("homepi-pcm-router not active");
  }
  if (!unit_active("mosquitto.service")) {
    result.failures.push_back("mosquitto not active");
  }
  if (!unit_active("homepi-hifi-serial.service")) {
    result.failures.push_back("homepi-hifi-serial not active");
  }
  if (!alsa_loopback_ready()) {
    result.failures.push_back("ALSA loopback cards HomePiZonesA/B not ready");
  }
  if (!db.controller_synced()) {
    result.failures.push_back("controller not synced");
  }
  if (!db.has_zone_data()) {
    result.failures.push_back("zone data missing");
  }
  if (!db.airplay_source_number().has_value()) {
    result.failures.push_back("airplay source not configured");
  }

  result.ready = result.failures.empty();
  return result;
}

}  // namespace homepi::shairport_sync
