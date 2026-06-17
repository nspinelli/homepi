#pragma once

#include <string>

#include "homepi/pcm-router/active-audio-config.hpp"

namespace homepi::pcm_router {

/** Loads active audio configuration from core storage or artifact fallback. */
class AudioProfileLoader {
 public:
  /**
   * Creates a loader.
   * @param database_path SQLite database path.
   * @param artifact_path Generated operating profile artifact path.
   */
  AudioProfileLoader(std::string database_path, std::string artifact_path);

  /**
   * Loads the active configuration.
   * @return Active audio config.
   */
  ActiveAudioConfig load() const;

 private:
  std::string database_path_;
  std::string artifact_path_;
};

}  // namespace homepi::pcm_router
