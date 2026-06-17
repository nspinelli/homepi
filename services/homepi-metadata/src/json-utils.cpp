#include "homepi/metadata/json-utils.hpp"

#include <sstream>
#include <cstdlib>

namespace {

std::size_t find_field(const std::string& json, const std::string& field) {
  const std::string key = "\"" + field + "\"";
  return json.find(key);
}

void append_escaped(std::ostringstream& out, const std::string& value) {
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
        break;
    }
  }
}

}  // namespace

namespace homepi::metadata {

int parse_int_field(const std::string& json, const std::string& field) {
  const auto pos = find_field(json, field);
  if (pos == std::string::npos) {
    return 0;
  }
  const auto colon = json.find(':', pos);
  if (colon == std::string::npos) {
    return 0;
  }
  return std::atoi(json.c_str() + colon + 1);
}

std::string parse_event_name(const std::string& line) {
  const auto pos = find_field(line, "event");
  if (pos == std::string::npos) {
    return {};
  }
  const auto quote_start = line.find('"', pos + 8);
  if (quote_start == std::string::npos) {
    return {};
  }
  const auto quote_end = line.find('"', quote_start + 1);
  if (quote_end == std::string::npos) {
    return {};
  }
  return line.substr(quote_start + 1, quote_end - quote_start - 1);
}

std::string parse_payload_json(const std::string& line) {
  const auto pos = find_field(line, "payload");
  if (pos == std::string::npos) {
    return "{}";
  }
  const auto start = line.find('{', pos);
  if (start == std::string::npos) {
    return "{}";
  }
  int depth = 0;
  for (std::size_t i = start; i < line.size(); ++i) {
    if (line[i] == '{') {
      ++depth;
    } else if (line[i] == '}') {
      --depth;
      if (depth == 0) {
        return line.substr(start, i - start + 1);
      }
    }
  }
  return "{}";
}

std::string escape_json_string(const std::string& value) {
  std::ostringstream out;
  append_escaped(out, value);
  return out.str();
}

}  // namespace homepi::metadata
