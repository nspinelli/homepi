#include "homepi/usb-devices/service-health-emitter.hpp"

#include <sstream>

#include "homepi/events/event-emitter.hpp"
#include "homepi/usb-devices/json-utils.hpp"

namespace homepi::usb_devices {

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
  std::ostringstream extra;
  extra << "\"reachable\":true,"
        << "\"assignmentsDegraded\":" << (health.assignments_degraded ? "true" : "false")
        << ",\"connectedDeviceCount\":" << health.connected_device_count
        << ",\"udevMonitorActive\":" << (health.udev_monitor_active ? "true" : "false");

  homepi::events::EventEmitter emitter("homepi-usb-devices",
                                       [&](const std::string& line) { server.broadcast(line); });
  emitter.emit_service_status(event_name, correlation_id, status, extra.str());
}

}  // namespace homepi::usb_devices
