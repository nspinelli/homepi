#pragma once

#include <cstdint>
#include <optional>
#include <string>
#include <vector>

namespace homepi::storage {

/** PCM sample format supported by HomePi audio services. */
enum class SampleFormat { S16Le, S32Le };

/**
 * Converts a sample format enum to ALSA-style string.
 * @param format Sample format.
 * @return Format string such as S16_LE.
 */
inline const char* sample_format_to_string(SampleFormat format) {
  return format == SampleFormat::S32Le ? "S32_LE" : "S16_LE";
}

/**
 * Parses a sample format string.
 * @param value Format string.
 * @return Parsed format or nullopt.
 */
inline std::optional<SampleFormat> parse_sample_format(const std::string& value) {
  if (value == "S16_LE") {
    return SampleFormat::S16Le;
  }
  if (value == "S32_LE") {
    return SampleFormat::S32Le;
  }
  return std::nullopt;
}

/** A single ALSA-legal PCM profile combination. */
struct AudioProfileTuple {
  uint32_t sample_rate = 44100;
  uint16_t channels = 2;
  SampleFormat sample_format = SampleFormat::S16Le;

  bool operator==(const AudioProfileTuple& other) const {
    return sample_rate == other.sample_rate && channels == other.channels &&
           sample_format == other.sample_format;
  }
};

/** Whether a primary DAC is assigned. */
enum class ProfileMode { NoDacAssigned, DacAssigned };

/** Whether the active profile is usable. */
enum class ProfileStatus { Active, PausedInvalid };

/** Profile origin in storage. */
enum class ProfileSource { UserSelected, PlatformPolicy };

/**
 * Converts profile source to schema string.
 * @param source Profile source.
 * @return user_selected or platform_policy.
 */
inline const char* profile_source_to_string(ProfileSource source) {
  return source == ProfileSource::UserSelected ? "user_selected" : "platform_policy";
}

/**
 * Parses profile source string.
 * @param value Source string.
 * @return Parsed source or nullopt.
 */
inline std::optional<ProfileSource> parse_profile_source(const std::string& value) {
  if (value == "user_selected") {
    return ProfileSource::UserSelected;
  }
  if (value == "platform_policy") {
    return ProfileSource::PlatformPolicy;
  }
  return std::nullopt;
}

/** Probed ALSA capabilities for one audio device. */
struct AudioCapabilities {
  std::string device_id;
  std::vector<AudioProfileTuple> supported_profile_tuples;
  std::string probed_at;
  std::optional<std::string> probe_error;
};

/** Active audio configuration loaded from storage or artifact. */
struct ActiveAudioConfig {
  ProfileMode mode = ProfileMode::NoDacAssigned;
  ProfileStatus status = ProfileStatus::Active;
  AudioProfileTuple loopback_profile{};
  std::optional<AudioProfileTuple> dac_profile;
  std::string alsa_dac_device;
  std::string primary_device_id;
  uint64_t profile_revision = 0;
  ProfileSource profile_source = ProfileSource::PlatformPolicy;
};

/** Platform loopback default when no DAC is assigned. */
inline AudioProfileTuple platform_loopback_default() {
  return AudioProfileTuple{.sample_rate = 44100, .channels = 2, .sample_format = SampleFormat::S16Le};
}

}  // namespace homepi::storage
