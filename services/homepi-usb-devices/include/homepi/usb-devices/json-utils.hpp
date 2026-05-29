#pragma once

#include <sstream>
#include <string>
#include <string_view>
#include <vector>

#include "homepi/usb-devices/types.hpp"

namespace homepi::usb_devices {

/**
 * Escapes a string for JSON output.
 * @param value Raw value.
 * @return Escaped JSON string contents without surrounding quotes.
 */
inline std::string json_escape(std::string_view value) {
  std::ostringstream out;
  for (char ch : value) {
    switch (ch) {
      case '"':
        out << "\\\"";
        break;
      case '\\':
        out << "\\\\";
        break;
      case '\n':
        out << "\\n";
        break;
      case '\r':
        out << "\\r";
        break;
      case '\t':
        out << "\\t";
        break;
      default:
        if (static_cast<unsigned char>(ch) < 0x20) {
          out << "\\u"
              << std::hex << std::uppercase;
          const int code = static_cast<unsigned char>(ch);
          if (code < 0x10) {
            out << "000";
          } else if (code < 0x100) {
            out << "00";
          }
          out << code << std::dec;
        } else {
          out << ch;
        }
    }
  }
  return out.str();
}

/**
 * Extracts a JSON string field value (simple parser).
 * @param json JSON object text.
 * @param field Field name.
 * @return Field value or empty when missing.
 */
inline std::string json_get_string(std::string_view json, std::string_view field) {
  const std::string key = "\"" + std::string(field) + "\"";
  const auto key_pos = json.find(key);
  if (key_pos == std::string_view::npos) {
    return "";
  }
  const auto colon = json.find(':', key_pos + key.size());
  if (colon == std::string_view::npos) {
    return "";
  }
  auto pos = colon + 1;
  while (pos < json.size() && (json[pos] == ' ' || json[pos] == '\t')) {
    ++pos;
  }
  if (pos >= json.size()) {
    return "";
  }
  if (json[pos] == 'n') {
    return "";
  }
  if (json[pos] != '"') {
    return "";
  }
  ++pos;
  std::string out;
  while (pos < json.size()) {
    const char ch = json[pos++];
    if (ch == '\\' && pos < json.size()) {
      const char next = json[pos++];
      if (next == 'n') {
        out.push_back('\n');
      } else if (next == 't') {
        out.push_back('\t');
      } else if (next == 'r') {
        out.push_back('\r');
      } else if (next == '"') {
        out.push_back('"');
      } else if (next == '\\') {
        out.push_back('\\');
      } else {
        out.push_back(next);
      }
      continue;
    }
    if (ch == '"') {
      break;
    }
    out.push_back(ch);
  }
  return out;
}

/**
 * Serializes a USB device to JSON object text.
 * @param device Device record.
 * @return JSON object.
 */
inline std::string device_to_json(const UsbDevice& device) {
  std::ostringstream out;
  out << "{"
      << "\"deviceId\":\"" << json_escape(device.device_id) << "\","
      << "\"displayName\":\"" << json_escape(device.display_name) << "\","
      << "\"kind\":\"" << device_kind_to_string(device.kind) << "\","
      << "\"present\":" << (device.present ? "true" : "false");
  if (!device.id_vendor.empty()) {
    out << ",\"idVendor\":\"" << json_escape(device.id_vendor) << "\"";
  }
  if (!device.id_product.empty()) {
    out << ",\"idProduct\":\"" << json_escape(device.id_product) << "\"";
  }
  if (!device.serial.empty()) {
    out << ",\"serial\":\"" << json_escape(device.serial) << "\"";
  }
  if (!device.devpath.empty()) {
    out << ",\"devpath\":\"" << json_escape(device.devpath) << "\"";
  }
  if (!device.alsa_card.empty()) {
    out << ",\"alsaCard\":\"" << json_escape(device.alsa_card) << "\"";
  }
  if (!device.resolved_alsa_name.empty()) {
    out << ",\"resolvedAlsaName\":\"" << json_escape(device.resolved_alsa_name) << "\"";
  }
  out << "}";
  return out.str();
}

/**
 * Serializes assignments to JSON.
 * @param assignments Role assignments.
 * @return JSON object.
 */
inline std::string assignments_to_json(const UsbAssignments& assignments) {
  std::ostringstream out;
  out << "{";
  auto emit = [&](const char* key, const std::optional<std::string>& value) {
    out << "\"" << key << "\":";
    if (value && !value->empty()) {
      out << "\"" << json_escape(*value) << "\"";
    } else {
      out << "null";
    }
  };
  emit("serial", assignments.serial);
  out << ",";
  emit("audioPrimary", assignments.audio_primary);
  out << ",";
  emit("paging", assignments.paging);
  out << "}";
  return out.str();
}

/**
 * Serializes a device list response.
 * @param devices Devices to include.
 * @return JSON object with devices array.
 */
inline std::string devices_response_json(const std::vector<UsbDevice>& devices) {
  std::ostringstream out;
  out << "{\"devices\":[";
  for (std::size_t i = 0; i < devices.size(); ++i) {
    if (i > 0) {
      out << ",";
    }
    out << device_to_json(devices[i]);
  }
  out << "]}";
  return out.str();
}

}  // namespace homepi::usb_devices
