#include "homepi/audio-paging/json-utils.hpp"

#include <sstream>

namespace homepi::audio_paging {

std::string json_escape(std::string_view value) {
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
        break;
    }
  }
  return out.str();
}

std::string json_get_string(std::string_view json, std::string_view field) {
  const std::string key = "\"" + std::string(field) + "\"";
  const auto key_pos = json.find(key);
  if (key_pos == std::string_view::npos) {
    return {};
  }
  const auto colon = json.find(':', key_pos + key.size());
  if (colon == std::string_view::npos) {
    return {};
  }
  std::size_t pos = colon + 1;
  while (pos < json.size() && (json[pos] == ' ' || json[pos] == '\t')) {
    ++pos;
  }
  if (pos >= json.size() || json[pos] != '"') {
    return {};
  }
  ++pos;
  std::string out;
  while (pos < json.size()) {
    char ch = json[pos++];
    if (ch == '\\' && pos < json.size()) {
      const char escaped = json[pos++];
      switch (escaped) {
        case 'n':
          out.push_back('\n');
          break;
        case 'r':
          out.push_back('\r');
          break;
        case 't':
          out.push_back('\t');
          break;
        case '\\':
          out.push_back('\\');
          break;
        case '"':
          out.push_back('"');
          break;
        default:
          out.push_back(escaped);
          break;
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

std::string json_get_scalar(std::string_view json, std::string_view field) {
  const std::string string_value = json_get_string(json, field);
  if (!string_value.empty()) {
    return string_value;
  }
  const std::string key = "\"" + std::string(field) + "\"";
  const auto key_pos = json.find(key);
  if (key_pos == std::string_view::npos) {
    return {};
  }
  const auto colon = json.find(':', key_pos + key.size());
  if (colon == std::string_view::npos) {
    return {};
  }
  std::size_t pos = colon + 1;
  while (pos < json.size() && (json[pos] == ' ' || json[pos] == '\t')) {
    ++pos;
  }
  const std::size_t start = pos;
  while (pos < json.size()) {
    const char ch = json[pos];
    if (ch == ',' || ch == '}' || ch == '\n' || ch == '\r') {
      break;
    }
    ++pos;
  }
  if (pos <= start) {
    return {};
  }
  return std::string(json.substr(start, pos - start));
}

std::string json_get_object(std::string_view json, std::string_view field) {
  const std::string key = "\"" + std::string(field) + "\"";
  const auto key_pos = json.find(key);
  if (key_pos == std::string_view::npos) {
    return "{}";
  }
  const auto start = json.find('{', key_pos + key.size());
  if (start == std::string_view::npos) {
    return "{}";
  }
  int depth = 0;
  for (std::size_t i = start; i < json.size(); ++i) {
    if (json[i] == '{') {
      ++depth;
    } else if (json[i] == '}') {
      --depth;
      if (depth == 0) {
        return std::string(json.substr(start, i - start + 1));
      }
    }
  }
  return "{}";
}

std::string parse_payload_json(const std::string& event_line) {
  return json_get_object(event_line, "payload");
}

std::string parse_event_name(const std::string& event_line) {
  return json_get_string(event_line, "event");
}

std::string parse_topic_name(const std::string& event_line) {
  return json_get_string(event_line, "topic");
}

std::string parse_correlation_id(const std::string& event_line) {
  const std::string correlation_id = json_get_string(event_line, "correlationId");
  return correlation_id.empty() ? "audio-paging" : correlation_id;
}

std::string idle_policy_to_string(PagingIdlePolicy policy) {
  return policy == PagingIdlePolicy::AlwaysWarm ? "always_warm" : "warm_with_timeout";
}

PagingIdlePolicy parse_idle_policy(const std::string& value, PagingIdlePolicy fallback) {
  if (value == "always_warm") {
    return PagingIdlePolicy::AlwaysWarm;
  }
  if (value == "warm_with_timeout") {
    return PagingIdlePolicy::WarmWithTimeout;
  }
  return fallback;
}

std::string resource_state_to_string(ResourceState state) {
  switch (state) {
    case ResourceState::Disabled:
      return "DISABLED";
    case ResourceState::Cold:
      return "COLD";
    case ResourceState::Warm:
      return "WARM";
    case ResourceState::Active:
      return "ACTIVE";
  }
  return "COLD";
}

std::string paging_status_to_json(const PagingStatus& status) {
  std::ostringstream out;
  out << "{"
      << "\"ready\":" << (status.ready ? "true" : "false") << ","
      << "\"resourceState\":\"" << resource_state_to_string(status.resource_state) << "\","
      << "\"dacConnected\":" << (status.dac_connected ? "true" : "false") << ","
      << "\"dacOpen\":" << (status.dac_open ? "true" : "false") << ","
      << "\"voiceLoaded\":" << (status.voice_loaded ? "true" : "false") << ","
      << "\"hifiConnected\":" << (status.hifi_connected ? "true" : "false") << ","
      << "\"busy\":" << (status.busy ? "true" : "false") << ","
      << "\"pagingDacDeviceId\":";
  if (status.paging_dac_device_id.empty()) {
    out << "null";
  } else {
    out << "\"" << json_escape(status.paging_dac_device_id) << "\"";
  }
  out << ",\"alsaDevice\":\"" << json_escape(status.alsa_device) << "\"";
  if (status.paging_alsa_card >= 0) {
    out << ",\"pagingAlsaCard\":" << status.paging_alsa_card;
  } else {
    out << ",\"pagingAlsaCard\":null";
  }
  if (status.dac_output_volume_percent >= 0) {
    out << ",\"dacOutputVolumePercent\":" << status.dac_output_volume_percent;
  } else {
    out << ",\"dacOutputVolumePercent\":null";
  }
  out << "}";
  return out.str();
}

}  // namespace homepi::audio_paging
