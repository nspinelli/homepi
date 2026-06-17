#pragma once

#include <optional>
#include <string>
#include <vector>

#include "homepi/storage/audio-profile-types.hpp"
#include "homepi/usb-devices/types.hpp"

namespace homepi::usb_devices {

/** Probes ALSA playback capabilities for USB audio devices. */
class AlsaCapabilityProbe {
 public:
  /**
   * Probes supported profile tuples for a playback device.
   * @param alsa_hw_name ALSA device such as hw:2,0.
   * @return Capabilities or nullopt when open fails.
   */
  std::optional<homepi::storage::AudioCapabilities> probe_playback(
      const std::string& device_id, const std::string& alsa_hw_name) const;
};

}  // namespace homepi::usb_devices
