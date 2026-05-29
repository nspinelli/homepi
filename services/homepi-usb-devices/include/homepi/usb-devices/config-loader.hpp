#pragma once

#include <string>

#include "homepi/usb-devices/types.hpp"

namespace homepi::usb_devices {

/**
 * Loads service configuration from a JSON file path.
 * @param config_path Path to service-config.json.
 * @return Parsed configuration.
 */
ServiceConfig load_service_config(const std::string& config_path);

}  // namespace homepi::usb_devices
