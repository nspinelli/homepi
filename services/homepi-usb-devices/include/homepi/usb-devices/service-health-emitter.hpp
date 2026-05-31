#pragma once

#include <string>

#include "homepi/usb-devices/types.hpp"
#include "homepi/usb-devices/unix-api-server.hpp"

namespace homepi::usb_devices {

/**
 * Maps service health to dashboard status string.
 * @param health Current health snapshot.
 * @return healthy, degraded, or offline.
 */
std::string dashboard_status(const ServiceHealth& health);

/**
 * Emits a system.service health envelope to all socket subscribers.
 * @param server Unix API server for broadcast.
 * @param health Current health snapshot.
 * @param event_name Lifecycle event name.
 * @param correlation_id Correlation identifier.
 */
void emit_service_health(UnixApiServer& server, const ServiceHealth& health,
                         const std::string& event_name, const std::string& correlation_id);

}  // namespace homepi::usb_devices
