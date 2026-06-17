#include "homepi/pcm-router/json-utils.hpp"

namespace homepi::pcm_router {

namespace {

std::size_t find_field(const std::string& json, const std::string& field) {
  const std::string key = "\"" + field + "\"";
  return json.find(key);
}

}  // namespace

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

std::vector<int> parse_int_array(const std::string& json, const std::string& field) {
  std::vector<int> values;
  const auto pos = find_field(json, field);
  if (pos == std::string::npos) {
    return values;
  }
  const auto start = json.find('[', pos);
  const auto end = json.find(']', start);
  if (start == std::string::npos || end == std::string::npos || end <= start) {
    return values;
  }
  std::string slice = json.substr(start + 1, end - start - 1);
  std::size_t cursor = 0;
  while (cursor < slice.size()) {
    const auto comma = slice.find(',', cursor);
    const std::string token =
        comma == std::string::npos ? slice.substr(cursor) : slice.substr(cursor, comma - cursor);
    if (!token.empty()) {
      values.push_back(std::atoi(token.c_str()));
    }
    if (comma == std::string::npos) {
      break;
    }
    cursor = comma + 1;
  }
  return values;
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

}  // namespace homepi::pcm_router
