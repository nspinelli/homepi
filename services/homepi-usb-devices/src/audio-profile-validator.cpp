#include "homepi/usb-devices/audio-profile-validator.hpp"

namespace homepi::usb_devices {

bool AudioProfileValidator::tuple_supported(
    const homepi::storage::AudioCapabilities& capabilities,
    const homepi::storage::AudioProfileTuple& tuple) {
  for (const auto& supported : capabilities.supported_profile_tuples) {
    if (supported == tuple) {
      return true;
    }
  }
  return false;
}

std::string AudioProfileValidator::validate_assignment_profile(
    const UsbAssignments& assignments,
    const std::optional<homepi::storage::AudioCapabilities>& capabilities) {
  if (!assignments.audio_primary || assignments.audio_primary->empty()) {
    if (assignments.audio_primary_profile.has_value()) {
      return "audioPrimaryProfile must be null when audioPrimary is not assigned";
    }
    return "";
  }

  if (!assignments.audio_primary_profile.has_value()) {
    return "audioPrimaryProfile is required when audioPrimary is assigned";
  }

  if (!capabilities.has_value()) {
    return "Audio capabilities are not available for the selected device";
  }

  if (!tuple_supported(*capabilities, *assignments.audio_primary_profile)) {
    return "audioPrimaryProfile is not supported by the selected device";
  }

  return "";
}

}  // namespace homepi::usb_devices
