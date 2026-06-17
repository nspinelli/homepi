#pragma once

#include <optional>
#include <string>

#include "homepi/storage/audio-profile-types.hpp"

namespace homepi::storage {

class DatabaseConnection;

/** Read-only access to audio capabilities and operating profiles. */
class AudioProfileRepository {
 public:
  explicit AudioProfileRepository(DatabaseConnection& db);

  /**
   * Loads probed capabilities for a device.
   * @param device_id Device identifier.
   * @return Capabilities when present.
   */
  std::optional<AudioCapabilities> get_capabilities(const std::string& device_id) const;

  /**
   * Loads the active audio configuration for consumers.
   * @return Active config derived from operating profile tables.
   */
  ActiveAudioConfig load_active_config() const;

  /**
   * Loads active config from a generated artifact JSON file.
   * @param artifact_path Artifact path.
   * @return Active config or nullopt.
   */
  static std::optional<ActiveAudioConfig> load_from_artifact(const std::string& artifact_path);

 private:
  DatabaseConnection& db_;
};

}  // namespace homepi::storage
