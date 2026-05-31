#include "homepi/usb-devices/service-health-emitter.hpp"

#include <chrono>
#include <iomanip>
#include <sstream>

#include "homepi/usb-devices/json-utils.hpp"

namespace homepi::usb_devices {

namespace {

std::string iso_timestamp() {
  const auto now = std::chrono::system_clock::now();
  const std::time_t t = std::chrono::system_clock::to_time_t(now);
  std::tm tm{};
  gmtime_r(&t, &tm);
  std::ostringstream out;
  out << std::put_time(&tm, "%Y-%m-%dT%H:%M:%S") << ".000Z";
  return out.str();
}

}  // namespace

std::string dashboard_status(const ServiceHealth& health) {
  if (health.lifecycle == "stopped" || health.lifecycle == "failed") {
    return "offline";
  }
  if (health.assignments_degraded) {
    return "degraded";
  }
  return "healthy";
}

void emit_service_health(UnixApiServer& server, const ServiceHealth& health,
                         const std::string& event_name, const std::string& correlation_id) {
  const std::string status = dashboard_status(health);
  std::ostringstream payload;
  payload << "{"
          << "\"status\":\"" << json_escape(status) << "\","
          << "\"reachable\":true,"
          << "\"assignmentsDegraded\":" << (health.assignments_degraded ? "true" : "false")
          << ",\"connectedDeviceCount\":" << health.connected_device_count
          << ",\"udevMonitorActive\":" << (health.udev_monitor_active ? "true" : "false")
          << "}";

  static unsigned long event_counter = 0;
  const unsigned long id = ++event_counter;

  std::ostringstream line;
  line << "{"
       << "\"version\":1,"
       << "\"id\":\"evt-usb-svc-" << id << "\","
       << "\"source\":\"homepi-usb-devices\","
       << "\"topic\":\"system.service\","
       << "\"event\":\"" << json_escape(event_name) << "\","
       << "\"correlationId\":\"" << json_escape(correlation_id) << "\","
       << "\"timestamp\":\"" << iso_timestamp() << "\","
       << "\"payload\":" << payload.str()
       << "}";

  server.broadcast(line.str());
}

}  // namespace homepi::usb_devices
