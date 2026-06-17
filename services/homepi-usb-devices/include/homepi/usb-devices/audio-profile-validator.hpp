#pragma once

#include <string>

#include "homepi/usb-devices/types.hpp"

#include "homepi/storage/audio-profile-types.hpp"

namespace homepi::usb_devices {

/** Validates user-selected profile tuples against probed capabilities. */
class AudioProfileValidator {
 public:
  /**
   * Returns true when tuple exists in capabilities.
   * @param capabilities Probed capabilities.
   * @param tuple Candidate tuple.
   * @return Validation result.
   */
  static bool tuple_supported(const homepi::storage::AudioCapabilities& capabilities,
                              const homepi::storage::AudioProfileTuple& tuple);

  /**
   * Returns an error message when assignment/profile combination is invalid.
   * @param assignments Assignments to validate.
   * @param capabilities Capabilities for assigned audio device.
   * @return Error message or empty string when valid.
   */
  static std::string validate_assignment_profile(
      const UsbAssignments& assignments,
      const std::optional<homepi::storage::AudioCapabilities>& capabilities);
};

}  // namespace homepi::usb_devices
