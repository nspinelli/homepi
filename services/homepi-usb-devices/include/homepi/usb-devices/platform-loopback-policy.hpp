#pragma once

#include "homepi/storage/audio-profile-types.hpp"

namespace homepi::usb_devices {

/** Platform loopback profile when no primary DAC is assigned. */
class PlatformLoopbackPolicy {
 public:
  /**
   * Returns the default loopback profile.
   * @return 44100 / S16_LE / stereo.
   */
  homepi::storage::AudioProfileTuple default_loopback_profile() const;
};

}  // namespace homepi::usb_devices
