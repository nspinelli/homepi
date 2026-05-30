#pragma once

#include <map>
#include <string>

#include "homepi/shairport-sync/types.hpp"

namespace homepi::shairport_sync {

/**
 * Generates Shairport Sync zone configuration files and hook scripts.
 */
class ConfigGenerator {
 public:
  /**
   * @param config Service configuration.
   */
  explicit ConfigGenerator(ServiceConfig config);

  /**
   * Writes zone configs and hooks; returns map of zone -> content hash.
   * @param zones Zone rows.
   * @param settings Per-zone settings.
   * @param airplay_source AirPlay source number for session hooks.
   * @returns Zone number to SHA256 hash of generated config.
   */
  std::map<int, std::string> generate(const std::vector<ZoneRow>& zones,
                                      const std::vector<ZoneSettings>& settings,
                                      int airplay_source) const;

 private:
  ServiceConfig config_;
};

}  // namespace homepi::shairport_sync
