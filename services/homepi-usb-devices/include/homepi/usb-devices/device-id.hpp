#pragma once

#include <string>

namespace homepi::usb_devices {

/**
 * Builds a stable USB device identifier from udev attributes.
 * @param id_vendor USB vendor id hex.
 * @param id_product USB product id hex.
 * @param serial Serial string when present.
 * @param devpath udev DEVPATH fallback.
 * @return Stable deviceId string.
 */
std::string build_device_id(const std::string& id_vendor, const std::string& id_product,
                            const std::string& serial, const std::string& devpath);

}  // namespace homepi::usb_devices
