#include "homepi/pcm-router/audio-profile-loader.hpp"

#include <filesystem>

#include "homepi/storage/audio-profile-repository.hpp"
#include "homepi/storage/database-connection.hpp"

namespace homepi::pcm_router {

AudioProfileLoader::AudioProfileLoader(std::string database_path, std::string artifact_path)
    : database_path_(std::move(database_path)), artifact_path_(std::move(artifact_path)) {}

ActiveAudioConfig AudioProfileLoader::load() const {
  ActiveAudioConfig config;
  if (std::filesystem::exists(database_path_)) {
    try {
      homepi::storage::DatabaseConnection db(database_path_,
                                             homepi::storage::DatabaseOpenMode::ReadOnly);
      homepi::storage::AudioProfileRepository repo(db);
      const homepi::storage::ActiveAudioConfig stored = repo.load_active_config();
      config.mode = stored.mode;
      config.status = stored.status;
      config.loopback_profile = stored.loopback_profile;
      config.dac_profile = stored.dac_profile;
      config.alsa_dac_device = stored.alsa_dac_device;
      config.profile_revision = stored.profile_revision;
      config.profile_source = stored.profile_source;
      return config;
    } catch (...) {
    }
  }

  const auto artifact = homepi::storage::AudioProfileRepository::load_from_artifact(artifact_path_);
  if (!artifact.has_value()) {
    config.loopback_profile = homepi::storage::platform_loopback_default();
    return config;
  }

  config.mode = artifact->mode;
  config.status = artifact->status;
  config.loopback_profile = artifact->loopback_profile;
  config.dac_profile = artifact->dac_profile;
  config.alsa_dac_device = artifact->alsa_dac_device;
  config.profile_revision = artifact->profile_revision;
  config.profile_source = artifact->profile_source;
  return config;
}

}  // namespace homepi::pcm_router
