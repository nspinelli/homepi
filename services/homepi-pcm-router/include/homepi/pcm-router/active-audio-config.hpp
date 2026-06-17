#pragma once

#include <optional>
#include <string>

#include "homepi/storage/audio-profile-types.hpp"

namespace homepi::pcm_router {

using AudioProfileTuple = homepi::storage::AudioProfileTuple;
using ProfileMode = homepi::storage::ProfileMode;
using ProfileStatus = homepi::storage::ProfileStatus;
using ProfileSource = homepi::storage::ProfileSource;

/** In-memory active audio configuration for pcm-router runtime. */
struct ActiveAudioConfig {
  ProfileMode mode = ProfileMode::NoDacAssigned;
  ProfileStatus status = ProfileStatus::Active;
  AudioProfileTuple loopback_profile = homepi::storage::platform_loopback_default();
  std::optional<AudioProfileTuple> dac_profile;
  std::string alsa_dac_device;
  uint64_t profile_revision = 0;
  ProfileSource profile_source = ProfileSource::PlatformPolicy;
};

}  // namespace homepi::pcm_router
