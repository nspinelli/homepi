#pragma once

#include <vector>

#include "homepi/usb-devices/types.hpp"

namespace homepi::usb_devices {

/**
 * Scans connected USB serial and audio devices via libudev.
 * @return Detected device records marked present.
 */
std::vector<UsbDevice> scan_usb_devices();

}  // namespace homepi::usb_devices
