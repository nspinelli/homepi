#pragma once

#include <optional>
#include <string>

namespace homepi::audio_orchestrator {

/**
 * Loads the configured AirPlay source number from SQLite.
 */
class AirplaySourceLoader {
 public:
  /**
   * Creates a loader for the given database path.
   * @param database_path SQLite database path.
   */
  explicit AirplaySourceLoader(std::string database_path);

  /**
   * Reads the source marked is_airplay=1.
   * @returns Source number or nullopt when unset.
   */
  std::optional<int> load_from_database() const;

 private:
  std::string database_path_;
};

}  // namespace homepi::audio_orchestrator
