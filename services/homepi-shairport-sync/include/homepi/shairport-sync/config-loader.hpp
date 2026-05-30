#pragma once

#include <string>

#include "homepi/shairport-sync/types.hpp"

namespace homepi::shairport_sync {

/**
 * Loads service configuration from JSON and environment overrides.
 * @param config_path Path to service-config.json.
 * @returns Parsed service configuration.
 */
ServiceConfig load_service_config(const std::string& config_path);

}  // namespace homepi::shairport_sync
