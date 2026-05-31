#include "homepi/hifi-serial/service-health-emitter.hpp"

#include <chrono>
#include <iomanip>
#include <sstream>

#include "homepi/hifi-serial/json-utils.hpp"

namespace homepi::hifi_serial {

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
  if (!health.connected) {
    return "offline";
  }
  if (health.degraded || health.sync_in_progress) {
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
          << "\"connected\":" << (health.connected ? "true" : "false") << ","
          << "\"degraded\":" << (health.degraded ? "true" : "false") << ","
          << "\"syncInProgress\":" << (health.sync_in_progress ? "true" : "false") << ","
          << "\"serialAssigned\":" << (health.serial_assigned ? "true" : "false") << ","
          << "\"queueDepth\":" << health.queue_depth
          << "}";

  static unsigned long event_counter = 0;
  const unsigned long id = ++event_counter;

  std::ostringstream line;
  line << "{"
       << "\"version\":1,"
       << "\"id\":\"evt-hifi-svc-" << id << "\","
       << "\"source\":\"homepi-hifi-serial\","
       << "\"topic\":\"system.service\","
       << "\"event\":\"" << json_escape(event_name) << "\","
       << "\"correlationId\":\"" << json_escape(correlation_id) << "\","
       << "\"timestamp\":\"" << iso_timestamp() << "\","
       << "\"payload\":" << payload.str()
       << "}";

  server.broadcast(line.str());
}

}  // namespace homepi::hifi_serial
