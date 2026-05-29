#include "homepi/usb-devices/device-scanner.hpp"

#include <algorithm>
#include <cctype>
#include <fstream>
#include <libudev.h>
#include <map>
#include <regex>
#include <set>
#include <sstream>

#include "homepi/usb-devices/device-id.hpp"

namespace homepi::usb_devices {

namespace {

/**
 * Returns a udev property value.
 * @param dev udev device.
 * @param key Property key.
 * @return Property value or empty string.
 */
std::string udev_prop(udev_device* dev, const char* key) {
  if (dev == nullptr) {
    return "";
  }
  const char* value = udev_device_get_property_value(dev, key);
  return value != nullptr ? value : "";
}

/**
 * Finds the parent USB device node for a udev device.
 * @param dev Starting device.
 * @return USB device parent or nullptr.
 */
udev_device* find_usb_device_parent(udev_device* dev) {
  for (udev_device* current = dev; current != nullptr;
       current = udev_device_get_parent(current)) {
    const char* subsystem = udev_device_get_subsystem(current);
    const char* devtype = udev_device_get_devtype(current);
    if (subsystem != nullptr && std::string(subsystem) == "usb" && devtype != nullptr &&
        std::string(devtype) == "usb_device") {
      return current;
    }
  }
  return nullptr;
}

/**
 * Collects identity fields from a USB device node.
 * @param usb_dev USB udev device.
 * @param vendor_out Vendor hex id.
 * @param product_out Product hex id.
 * @param serial_out Serial string.
 * @param devpath_out Device devpath.
 */
void identity_from_usb(udev_device* usb_dev, std::string& vendor_out, std::string& product_out,
                       std::string& serial_out, std::string& devpath_out) {
  vendor_out = udev_prop(usb_dev, "ID_VENDOR_ID");
  product_out = udev_prop(usb_dev, "ID_MODEL_ID");
  serial_out = udev_prop(usb_dev, "ID_SERIAL_SHORT");
  if (serial_out.empty()) {
    serial_out = udev_prop(usb_dev, "ID_SERIAL");
  }
  devpath_out =
      udev_device_get_devpath(usb_dev) != nullptr ? udev_device_get_devpath(usb_dev) : "";
}

/**
 * Trims ASCII whitespace from both ends.
 * @param value Input string.
 * @return Trimmed string.
 */
std::string trim(std::string value) {
  while (!value.empty() && std::isspace(static_cast<unsigned char>(value.front())) != 0) {
    value.erase(value.begin());
  }
  while (!value.empty() && std::isspace(static_cast<unsigned char>(value.back())) != 0) {
    value.pop_back();
  }
  return value;
}

/**
 * Parsed ALSA card metadata for USB audio devices.
 */
struct UsbAlsaCard {
  std::string card_index;
  std::string short_name;
  std::string header_title;
  std::string detail_line;
  std::string usb_port;
};

/**
 * Extracts the USB port segment from an ALSA card detail line.
 * @param detail ALSA detail line.
 * @return Port id such as xhci-hcd.0-1 or empty.
 */
std::string extract_usb_port(const std::string& detail) {
  static const std::regex pattern(R"(at (usb-[^,\s]+))");
  std::smatch match;
  if (std::regex_search(detail, match, pattern) && match.size() > 1) {
    return match[1].str();
  }
  return "";
}

/**
 * Loads USB ALSA cards from /proc/asound/cards.
 * @return USB card entries only.
 */
std::vector<UsbAlsaCard> load_usb_alsa_cards() {
  std::vector<UsbAlsaCard> cards;
  std::ifstream input("/proc/asound/cards");
  if (!input.is_open()) {
    return cards;
  }

  std::string line;
  static const std::regex header_pattern(R"(^\s*(\d+)\s+\[([^\]]+)\]\s*:\s*(.+)$)");

  while (std::getline(input, line)) {
    if (line.empty()) {
      continue;
    }

    std::smatch header_match;
    if (!std::regex_match(line, header_match, header_pattern)) {
      continue;
    }

    UsbAlsaCard card;
    card.card_index = header_match[1].str();
    card.short_name = trim(header_match[2].str());
    card.header_title = trim(header_match[3].str());

    if (!std::getline(input, line)) {
      break;
    }
    card.detail_line = trim(line);
    const std::string haystack = card.header_title + " " + card.detail_line;
    if (haystack.find("USB") == std::string::npos && haystack.find("usb-") == std::string::npos) {
      continue;
    }

    card.usb_port = extract_usb_port(card.detail_line);
    cards.push_back(std::move(card));
  }

  return cards;
}

/**
 * Builds a descriptive label for a USB ALSA card.
 * @param card ALSA card metadata.
 * @param vendor USB vendor name.
 * @param product USB product name.
 * @param serial USB serial.
 * @return Display label.
 */
std::string build_audio_display_name(const UsbAlsaCard& card, const std::string& vendor,
                                   const std::string& product, const std::string& serial) {
  std::ostringstream out;
  if (!vendor.empty() || !product.empty()) {
    out << trim(vendor + " " + product);
  } else if (!card.detail_line.empty()) {
    const auto at_pos = card.detail_line.find(" at usb-");
    out << trim(at_pos == std::string::npos ? card.detail_line
                                            : card.detail_line.substr(0, at_pos));
  } else {
    out << card.header_title;
  }

  if (!card.usb_port.empty()) {
    out << " — " << card.usb_port;
  } else if (!card.short_name.empty()) {
    out << " — " << card.short_name;
  }

  out << " (card " << card.card_index << ")";
  if (!serial.empty()) {
    out << " [" << serial << "]";
  }
  return out.str();
}

/**
 * Scans USB serial tty devices.
 * @param udev_ctx udev context.
 * @param seen_ids Seen device ids for deduplication.
 * @return Serial device records.
 */
std::vector<UsbDevice> scan_usb_serial_devices(udev* udev_ctx, std::set<std::string>& seen_ids) {
  std::vector<UsbDevice> devices;

  udev_enumerate* enumerate = udev_enumerate_new(udev_ctx);
  udev_enumerate_add_match_subsystem(enumerate, "tty");
  udev_enumerate_scan_devices(enumerate);

  udev_list_entry* entry = udev_enumerate_get_list_entry(enumerate);
  for (; entry != nullptr; entry = udev_list_entry_get_next(entry)) {
    const char* syspath = udev_list_entry_get_name(entry);
    udev_device* dev = udev_device_new_from_syspath(udev_ctx, syspath);
    if (dev == nullptr) {
      continue;
    }

    udev_device* usb_dev = find_usb_device_parent(dev);
    if (usb_dev == nullptr) {
      udev_device_unref(dev);
      continue;
    }

    std::string vendor;
    std::string product;
    std::string serial;
    std::string devpath;
    identity_from_usb(usb_dev, vendor, product, serial, devpath);

    if (vendor.empty() || product.empty()) {
      udev_device_unref(dev);
      continue;
    }

    const std::string device_id = build_device_id(vendor, product, serial, devpath);
    if (!seen_ids.insert(device_id).second) {
      udev_device_unref(dev);
      continue;
    }

    UsbDevice record;
    record.device_id = device_id;
    record.kind = DeviceKind::Serial;
    record.present = true;
    record.id_vendor = vendor;
    record.id_product = product;
    record.serial = serial;
    const char* devnode = udev_device_get_devnode(dev);
    record.devpath = devnode != nullptr ? devnode : devpath;

    const std::string vendor_name = udev_prop(usb_dev, "ID_VENDOR");
    const std::string model_name = udev_prop(usb_dev, "ID_MODEL");
    if (!vendor_name.empty() || !model_name.empty()) {
      record.display_name = trim(vendor_name + " " + model_name);
    } else {
      record.display_name = "USB Serial Adapter";
    }

    const char* tty_name = udev_device_get_sysname(dev);
    if (tty_name != nullptr) {
      record.display_name += " — /dev/" + std::string(tty_name);
    }
    if (!serial.empty()) {
      record.display_name += " [" + serial + "]";
    }

    devices.push_back(std::move(record));
    udev_device_unref(dev);
  }

  udev_enumerate_unref(enumerate);
  return devices;
}

/**
 * Scans USB audio devices from ALSA card entries.
 * @param udev_ctx udev context.
 * @param seen_ids Seen device ids for deduplication.
 * @return Audio device records.
 */
std::vector<UsbDevice> scan_usb_audio_devices(udev* udev_ctx, std::set<std::string>& seen_ids) {
  std::vector<UsbDevice> devices;
  const auto cards = load_usb_alsa_cards();

  for (const UsbAlsaCard& card : cards) {
    const std::string syspath = "/sys/class/sound/card" + card.card_index;
    udev_device* dev = udev_device_new_from_syspath(udev_ctx, syspath.c_str());
    if (dev == nullptr) {
      continue;
    }

    udev_device* usb_dev = find_usb_device_parent(dev);
    if (usb_dev == nullptr) {
      udev_device_unref(dev);
      continue;
    }

    std::string vendor;
    std::string product;
    std::string serial;
    std::string devpath;
    identity_from_usb(usb_dev, vendor, product, serial, devpath);

    if (vendor.empty() || product.empty()) {
      udev_device_unref(dev);
      continue;
    }

    const std::string device_id =
        build_device_id(vendor, product, serial, devpath) + ":alsa:" + card.card_index;
    if (!seen_ids.insert(device_id).second) {
      udev_device_unref(dev);
      continue;
    }

    UsbDevice record;
    record.device_id = device_id;
    record.kind = DeviceKind::Audio;
    record.present = true;
    record.id_vendor = vendor;
    record.id_product = product;
    record.serial = serial;
    record.devpath = devpath;
    record.alsa_card = card.card_index;
    record.resolved_alsa_name = "hw:" + card.card_index + ",0";
    record.display_name =
        build_audio_display_name(card, udev_prop(usb_dev, "ID_VENDOR"),
                                 udev_prop(usb_dev, "ID_MODEL"), serial);

    devices.push_back(std::move(record));
    udev_device_unref(dev);
  }

  return devices;
}

}  // namespace

std::vector<UsbDevice> scan_usb_devices() {
  std::vector<UsbDevice> devices;
  std::set<std::string> seen_ids;

  udev* udev_ctx = udev_new();
  if (udev_ctx == nullptr) {
    return devices;
  }

  auto serial = scan_usb_serial_devices(udev_ctx, seen_ids);
  auto audio = scan_usb_audio_devices(udev_ctx, seen_ids);

  devices.insert(devices.end(), serial.begin(), serial.end());
  devices.insert(devices.end(), audio.begin(), audio.end());

  udev_unref(udev_ctx);

  std::sort(devices.begin(), devices.end(),
            [](const UsbDevice& a, const UsbDevice& b) { return a.display_name < b.display_name; });
  return devices;
}

}  // namespace homepi::usb_devices
