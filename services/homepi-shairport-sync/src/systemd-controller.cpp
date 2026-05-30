#include "homepi/shairport-sync/systemd-controller.hpp"

#include <array>
#include <cstdio>
#include <memory>
#include <string>

namespace homepi::shairport_sync {

namespace {

void run_systemctl(const std::string& args) {
  const std::string cmd = "sudo /bin/systemctl " + args + " >/dev/null 2>&1";
  std::ignore = std::system(cmd.c_str());
}

}  // namespace

void SystemdController::stop_all_zones(int zone_count) const {
  for (int zone = 1; zone <= zone_count; ++zone) {
    run_systemctl("stop homepi-metadata@" + std::to_string(zone) + ".service");
    run_systemctl("stop homepi-shairport@" + std::to_string(zone) + ".service");
  }
}

void SystemdController::start_zones(const std::vector<int>& zone_numbers) const {
  for (int zone : zone_numbers) {
    run_systemctl("start homepi-shairport@" + std::to_string(zone) + ".service");
    run_systemctl("start homepi-metadata@" + std::to_string(zone) + ".service");
  }
}

void SystemdController::restart_zones(const std::vector<int>& zone_numbers) const {
  for (int zone : zone_numbers) {
    run_systemctl("restart homepi-shairport@" + std::to_string(zone) + ".service");
    run_systemctl("restart homepi-metadata@" + std::to_string(zone) + ".service");
  }
}

bool SystemdController::is_unit_active(const std::string& unit) const {
  const std::string cmd = "systemctl is-active " + unit + " 2>/dev/null";
  std::array<char, 64> buffer{};
  std::string output;
  const std::unique_ptr<FILE, decltype(&pclose)> pipe(popen(cmd.c_str(), "r"), pclose);
  if (!pipe) {
    return false;
  }
  while (fgets(buffer.data(), static_cast<int>(buffer.size()), pipe.get()) != nullptr) {
    output += buffer.data();
  }
  while (!output.empty() && (output.back() == '\n' || output.back() == '\r')) {
    output.pop_back();
  }
  return output == "active";
}

}  // namespace homepi::shairport_sync
