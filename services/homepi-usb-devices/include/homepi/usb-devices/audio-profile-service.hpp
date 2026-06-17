#pragma once

#include <string>

#include "homepi/usb-devices/alsa-capability-probe.hpp"
#include "homepi/usb-devices/assignment-repository.hpp"
#include "homepi/usb-devices/audio-profile-writer.hpp"
#include "homepi/usb-devices/types.hpp"
#include "homepi/usb-devices/usb-event-emitter.hpp"

namespace homepi::usb_devices {

/** Coordinates audio capability probing and operating profile persistence. */
class AudioProfileService {
 public:
  AudioProfileService(AssignmentRepository& repository, AudioProfileWriter& writer,
                      AlsaCapabilityProbe& probe, UsbEventEmitter& events,
                      std::string database_path, std::string generated_dir);

  /**
   * Probes and stores capabilities for all present audio devices.
   * @param devices Device list.
   */
  void refresh_audio_capabilities(const std::vector<UsbDevice>& devices);

  /**
   * Validates saved profile against stored capabilities when assignment exists.
   * @param assignments Current assignments.
   * @param correlation_id Correlation id for events.
   */
  void validate_active_profile(const UsbAssignments& assignments, const std::string& correlation_id);

  /**
   * Persists assignments and updates operating profiles.
   * @param assignments Assignments including optional profile tuple.
   * @param devices Device list.
   * @param correlation_id Request correlation id.
   * @param error_out Error message on failure.
   * @return True on success.
   */
  bool apply_assignments(const UsbAssignments& assignments, const std::vector<UsbDevice>& devices,
                         const std::string& correlation_id, std::string& error_out);

  /**
   * Serializes audio capabilities for a device.
   * @param device_id Device id.
   * @return JSON object string.
   */
  std::string capabilities_json(const std::string& device_id) const;

  /**
   * Serializes the active operating profile artifact view.
   * @return JSON object string.
   */
  std::string operating_profile_json() const;

 private:
  std::optional<homepi::storage::AudioCapabilities> load_capabilities(
      const std::string& device_id) const;

  AssignmentRepository& repository_;
  AudioProfileWriter& writer_;
  AlsaCapabilityProbe& probe_;
  UsbEventEmitter& events_;
  std::string database_path_;
  std::string generated_dir_;
};

}  // namespace homepi::usb_devices
