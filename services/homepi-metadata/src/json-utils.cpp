#include "homepi/metadata/json-utils.hpp"

#include <sstream>
#include <cstdlib>
#include <vector>

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

std::vector<int> parse_int_array_field(const std::string& json, const std::string& field) {
  std::vector<int> values;
  const auto key_pos = json.find("\"" + field + "\"");
  if (key_pos == std::string::npos) {
    return values;
  }
  const auto open = json.find('[', key_pos);
  const auto close = json.find(']', open == std::string::npos ? key_pos : open);
  if (open == std::string::npos || close == std::string::npos || close <= open) {
    return values;
  }
  std::string slice = json.substr(open + 1, close - open - 1);
  std::size_t pos = 0;
  while (pos < slice.size()) {
    while (pos < slice.size() && (slice[pos] == ' ' || slice[pos] == ',')) {
      ++pos;
    }
    if (pos >= slice.size()) {
      break;
    }
    const std::size_t end = slice.find_first_of(",]", pos);
    const std::string token =
        slice.substr(pos, end == std::string::npos ? std::string::npos : end - pos);
    try {
      const int value = std::stoi(token);
      if (value > 0) {
        values.push_back(value);
      }
    } catch (...) {
    }
    if (end == std::string::npos) {
      break;
    }
    pos = end + 1;
  }
  return values;
}

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
