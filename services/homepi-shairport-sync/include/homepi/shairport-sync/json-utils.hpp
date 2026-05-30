#pragma once

#include <sstream>
#include <string>
#include <string_view>

namespace homepi::shairport_sync {

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

}  // namespace homepi::shairport_sync
