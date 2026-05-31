#pragma once

#include <sstream>
#include <string>
#include <string_view>

namespace homepi::hifi_serial {

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
        out << ch;
    }
  }
  return out.str();
}

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
  if (pos >= json.size() || json[pos] == 'n') {
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
 * Parses an integer field from a JSON object string.
 * @param json JSON object text.
 * @param field Field name.
 * @returns Parsed integer or -1 when missing.
 */
inline int json_get_int(std::string_view json, std::string_view field) {
  const std::string key = "\"" + std::string(field) + "\"";
  const auto key_pos = json.find(key);
  if (key_pos == std::string_view::npos) {
    return -1;
  }
  const auto colon = json.find(':', key_pos + key.size());
  if (colon == std::string_view::npos) {
    return -1;
  }
  try {
    return std::stoi(std::string(json.substr(colon + 1)));
  } catch (...) {
    return -1;
  }
}

/**
 * Parses a floating-point field from a JSON object string.
 * @param json JSON object text.
 * @param field Field name.
 * @returns Parsed value or -1 when missing.
 */
inline double json_get_double(std::string_view json, std::string_view field) {
  const std::string key = "\"" + std::string(field) + "\"";
  const auto key_pos = json.find(key);
  if (key_pos == std::string_view::npos) {
    return -1.0;
  }
  const auto colon = json.find(':', key_pos + key.size());
  if (colon == std::string_view::npos) {
    return -1.0;
  }
  try {
    return std::stod(std::string(json.substr(colon + 1)));
  } catch (...) {
    return -1.0;
  }
}

}  // namespace homepi::hifi_serial
