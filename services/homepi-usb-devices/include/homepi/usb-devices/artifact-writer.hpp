#pragma once

#include <string>
#include <vector>

#include "homepi/usb-devices/types.hpp"

namespace homepi::usb_devices {

/** Writes udev rules and ALSA snippets for current assignments. */
class ArtifactWriter {
 public:
  /**
   * Creates an artifact writer.
   * @param config Service configuration.
   */
  explicit ArtifactWriter(const ServiceConfig& config);

  /**
   * Regenerates all artifacts from assignments and devices.
   * @param assignments Saved assignments.
   * @param devices Known devices.
   * @return True when all files were written.
   */
  bool regenerate(const UsbAssignments& assignments, const std::vector<UsbDevice>& devices);

 private:
  ServiceConfig config_;
};

}  // namespace homepi::usb_devices
