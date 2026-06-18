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

/**
 * Strips the ALSA card suffix from an audio device id.
 * @param device_id Full device id, optionally ending with :alsa:N.
 * @return Identity without the ALSA suffix.
 */
std::string strip_alsa_suffix(const std::string& device_id);

/**
 * Returns true when two device ids refer to the same USB identity.
 * @param left First device id.
 * @param right Second device id.
 * @return True when identities match ignoring ALSA suffix.
 */
bool device_identity_matches(const std::string& left, const std::string& right);

}  // namespace homepi::usb_devices
