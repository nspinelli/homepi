#pragma once

#include <string>

#include "homepi/hifi-serial/types.hpp"

namespace homepi::hifi_serial {

/**
 * Loads service configuration from JSON.
 * @param config_path Path to service-config.json.
 * @return Parsed configuration.
 */
ServiceConfig load_service_config(const std::string& config_path);

}  // namespace homepi::hifi_serial
