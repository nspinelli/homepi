#include "homepi/usb-devices/device-id.hpp"

#include <cstdint>
#include <iomanip>
#include <sstream>

namespace homepi::usb_devices {

namespace {

/**
 * Hashes a fallback key when USB serial is unavailable.
 * @param input Input string.
 * @return Hex hash string.
 */
std::string fnv1a_hex(std::string_view input) {
  std::uint64_t hash = 14695981039346656037ULL;
  for (unsigned char ch : input) {
    hash ^= ch;
    hash *= 1099511628211ULL;
  }
  std::ostringstream out;
  out << std::hex << std::setw(16) << std::setfill('0') << hash;
  return out.str();
}

}  // namespace

std::string build_device_id(const std::string& id_vendor, const std::string& id_product,
                            const std::string& serial, const std::string& devpath) {
  if (!id_vendor.empty() && !id_product.empty() && !serial.empty()) {
    return "usb:" + id_vendor + ":" + id_product + ":" + serial;
  }
  const std::string fallback_source =
      !devpath.empty() ? devpath : (id_vendor + ":" + id_product);
  return "usb:fallback:" + fnv1a_hex(fallback_source);
}

}  // namespace homepi::usb_devices
