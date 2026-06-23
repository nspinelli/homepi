#include "homepi/audio-orchestrator/json-utils.hpp"

#include <cstdlib>

namespace homepi::audio_orchestrator {

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

std::string parse_string_field(const std::string& json, const std::string& field) {
  const auto pos = find_field(json, field);
  if (pos == std::string::npos) {
    return {};
  }
  const auto quote_start = json.find('"', pos + field.size() + 2);
  if (quote_start == std::string::npos) {
    return {};
  }
  const auto quote_end = json.find('"', quote_start + 1);
  if (quote_end == std::string::npos) {
    return {};
  }
  return json.substr(quote_start + 1, quote_end - quote_start - 1);
}

std::string parse_event_name(const std::string& line) {
  return parse_string_field(line, "event");
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

}  // namespace homepi::audio_orchestrator
