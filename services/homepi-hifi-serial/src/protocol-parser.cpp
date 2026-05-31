#include "homepi/hifi-serial/protocol-parser.hpp"

#include <cctype>
#include <cstring>
#include <sstream>

#include "homepi/hifi-serial/json-utils.hpp"

namespace homepi::hifi_serial {

namespace {

ParsedUpdate make_update(const std::string& topic, const std::string& event, const std::string& payload) {
  return ParsedUpdate{.event_name = event, .topic = topic, .payload_json = payload};
}

std::string payload_obj(const std::string& body) { return "{" + body + "}"; }

int parse_int_suffix(const std::string& line, std::size_t prefix_len) {
  if (line.size() <= prefix_len) {
    return 0;
  }
  int value = 0;
  bool negative = false;
  std::size_t i = prefix_len;
  if (line[i] == '-') {
    negative = true;
    ++i;
  }
  while (i < line.size() && std::isdigit(static_cast<unsigned char>(line[i]))) {
    value = value * 10 + (line[i] - '0');
    ++i;
  }
  return negative ? -value : value;
}

std::optional<int> parse_zone_index(const std::string& line, std::size_t start) {
  if (start >= line.size() || line[start] != 'Z') {
    return std::nullopt;
  }
  ++start;
  if (start < line.size() && line[start] == '0') {
    return 0;
  }
  int zone = 0;
  while (start < line.size() && std::isdigit(static_cast<unsigned char>(line[start]))) {
    zone = zone * 10 + (line[start] - '0');
    ++start;
  }
  return zone;
}

std::optional<int> parse_source_index(const std::string& line, std::size_t start) {
  if (start >= line.size() || line[start] != 'S') {
    return std::nullopt;
  }
  ++start;
  if (start < line.size() && line[start] == '0') {
    return 0;
  }
  int source = 0;
  while (start < line.size() && std::isdigit(static_cast<unsigned char>(line[start]))) {
    source = source * 10 + (line[start] - '0');
    ++start;
  }
  return source;
}

std::optional<int> parse_group_index(const std::string& line, std::size_t start) {
  if (start >= line.size() || line[start] != 'G') {
    return std::nullopt;
  }
  ++start;
  if (start < line.size() && line[start] == '0') {
    return 0;
  }
  int group = 0;
  while (start < line.size() && std::isdigit(static_cast<unsigned char>(line[start]))) {
    group = group * 10 + (line[start] - '0');
    ++start;
  }
  return group;
}

std::size_t numeric_suffix_end(const std::string& line, std::size_t prefix_len) {
  if (line.size() <= prefix_len) {
    return prefix_len;
  }
  std::size_t i = prefix_len;
  if (line[i] == '-') {
    ++i;
  }
  while (i < line.size() && std::isdigit(static_cast<unsigned char>(line[i]))) {
    ++i;
  }
  return i;
}

bool bulk_marker_shadowed(const std::string& line, char prefix, int index, int max_index,
                          std::size_t pos) {
  if (max_index < 10) {
    return false;
  }
  const std::string marker = std::string(1, prefix) + std::to_string(index);
  for (int longer = max_index; longer > index; --longer) {
    const std::string longer_marker = std::string(1, prefix) + std::to_string(longer);
    if (longer_marker.size() > marker.size() &&
        pos + longer_marker.size() <= line.size() &&
        line.compare(pos, longer_marker.size(), longer_marker) == 0) {
      return true;
    }
  }
  return false;
}

/**
 * Returns true when `pos` begins another ZzKEYWORD / SsKEYWORD segment (empty name slot).
 * @param line - Full response line.
 * @param pos - Candidate value start index.
 * @param prefix - Z or S.
 * @param max_index - Highest valid index.
 * @param keyword - Protocol keyword (e.g. NAME).
 * @returns Whether the position is an immediate next marker, not a name value.
 */
bool value_is_immediate_prefixed_keyword(const std::string& line, std::size_t pos, char prefix,
                                         int max_index, const char* keyword) {
  if (pos >= line.size() || line[pos] != prefix) {
    return false;
  }

  const std::size_t keyword_len = std::strlen(keyword);
  std::size_t scan = pos + 1;
  if (scan >= line.size()) {
    return false;
  }

  int index = 0;
  if (line[scan] == '0') {
    return false;
  }
  if (!std::isdigit(static_cast<unsigned char>(line[scan]))) {
    return false;
  }
  while (scan < line.size() && std::isdigit(static_cast<unsigned char>(line[scan]))) {
    index = index * 10 + (line[scan] - '0');
    ++scan;
  }

  if (index <= 0 || index > max_index) {
    return false;
  }

  return scan + keyword_len <= line.size() && line.compare(scan, keyword_len, keyword) == 0;
}

void emit_indexed_numeric_bulk(std::vector<ParsedUpdate>& out, char prefix, int max_index,
                               const std::string& topic, const std::string& event,
                               const char* id_field, const char* value_field,
                               const std::string& line) {
  std::vector<std::pair<std::size_t, std::size_t>> consumed;

  for (int i = max_index; i >= 1; --i) {
    const std::string marker = std::string(1, prefix) + std::to_string(i);
    std::size_t search_from = 0;
    while (search_from < line.size()) {
      const auto pos = line.find(marker, search_from);
      if (pos == std::string::npos) {
        break;
      }
      search_from = pos + 1;
      if (bulk_marker_shadowed(line, prefix, i, max_index, pos)) {
        continue;
      }

      const std::size_t value_start = pos + marker.size();
      const std::size_t value_end = numeric_suffix_end(line, value_start);
      const bool overlaps = [&]() {
        for (const auto& span : consumed) {
          if (pos < span.second && value_end > span.first) {
            return true;
          }
        }
        return false;
      }();
      if (overlaps) {
        continue;
      }

      const int value = parse_int_suffix(line, value_start);
      std::ostringstream payload;
      payload << "\"" << id_field << "\":" << i << ",\"" << value_field << "\":" << value;
      out.push_back(make_update(topic, event, payload_obj(payload.str())));
      consumed.push_back({pos, value_end});
      break;
    }
  }
}

void emit_zone_bulk(std::vector<ParsedUpdate>& out, const std::string& field, const std::string& event,
                    const std::string& line, std::size_t /*prefix_len*/) {
  emit_indexed_numeric_bulk(out, 'Z', 16, "modules.audio.zone", event, "zone", field.c_str(), line);
}

/**
 * Parses ENABLE flag immediately after the ENABLE keyword (0 or 1 only).
 * @param line - Full response line.
 * @param enable_keyword_pos - Index of the ENABLE substring.
 * @returns 0/1 when valid, nullopt otherwise.
 */
std::optional<int> parse_enable_flag(const std::string& line, std::size_t enable_keyword_pos) {
  constexpr std::size_t kEnableLen = 6;
  const std::size_t val_pos = enable_keyword_pos + kEnableLen;
  if (val_pos >= line.size()) {
    return std::nullopt;
  }
  if (line[val_pos] == '0') {
    return 0;
  }
  if (line[val_pos] == '1') {
    return 1;
  }
  return std::nullopt;
}

/**
 * Parses bulk or interleaved ZzENABLEd / SsENABLEd segments (d is 0 or 1).
 * @param out - Parsed updates to append.
 * @param prefix - Z for zones, S for sources.
 * @param max_index - Highest zone/source index.
 * @param topic - Event topic.
 * @param event - Event name.
 * @param id_field - JSON id field (zone or source).
 * @param line - Full response line.
 */
void emit_prefixed_enable_bulk(std::vector<ParsedUpdate>& out, char prefix, int max_index,
                               const std::string& topic, const std::string& event,
                               const char* id_field, const std::string& line) {
  constexpr const char* kEnable = "ENABLE";
  std::vector<std::pair<std::size_t, std::size_t>> consumed;

  for (int index = max_index; index >= 1; --index) {
    const std::string marker = std::string(1, prefix) + std::to_string(index) + kEnable;
    std::size_t search_from = 0;
    while (search_from < line.size()) {
      const auto pos = line.find(marker, search_from);
      if (pos == std::string::npos) {
        break;
      }
      search_from = pos + 1;
      if (bulk_marker_shadowed(line, prefix, index, max_index, pos)) {
        continue;
      }

      const std::size_t val_pos = pos + marker.size();
      if (val_pos >= line.size()) {
        continue;
      }

      const char flag = line[val_pos];
      if (flag != '0' && flag != '1') {
        continue;
      }

      const std::size_t val_end = val_pos + 1;
      const bool overlaps = [&]() {
        for (const auto& span : consumed) {
          if (pos < span.second && val_end > span.first) {
            return true;
          }
        }
        return false;
      }();
      if (overlaps) {
        continue;
      }

      const int enabled = flag - '0';
      std::ostringstream payload;
      payload << "\"" << id_field << "\":" << index << ",\"enabled\":" << enabled;
      out.push_back(make_update(topic, event, payload_obj(payload.str())));
      consumed.push_back({pos, val_end});
      break;
    }
  }
}

/**
 * Parses bulk ZzKEYWORDnn / SsKEYWORDnn segments (e.g. Z7INIVOL35).
 * @param out - Parsed updates to append.
 * @param prefix - Z for zones, S for sources.
 * @param max_index - Highest zone/source index.
 * @param topic - Event topic.
 * @param event - Event name.
 * @param id_field - JSON id field (zone or source).
 * @param value_field - JSON value field name.
 * @param keyword - Protocol keyword after the index (e.g. INIVOL).
 * @param line - Full response line.
 */
void emit_prefixed_value_bulk(std::vector<ParsedUpdate>& out, char prefix, int max_index,
                              const std::string& topic, const std::string& event,
                              const char* id_field, const char* value_field, const char* keyword,
                              const std::string& line) {
  const std::size_t keyword_len = std::strlen(keyword);

  std::size_t pos = 0;
  while (pos < line.size()) {
    if (line[pos] != prefix) {
      ++pos;
      continue;
    }

    const std::size_t digit_pos = pos + 1;
    if (digit_pos >= line.size()) {
      ++pos;
      continue;
    }

    int index = 0;
    std::size_t scan = digit_pos;
    if (line[scan] == '0') {
      index = 0;
      ++scan;
    } else if (std::isdigit(static_cast<unsigned char>(line[scan]))) {
      while (scan < line.size() && std::isdigit(static_cast<unsigned char>(line[scan]))) {
        index = index * 10 + (line[scan] - '0');
        ++scan;
      }
    } else {
      ++pos;
      continue;
    }

    if (index <= 0 || index > max_index) {
      ++pos;
      continue;
    }

    if (scan + keyword_len > line.size() || line.compare(scan, keyword_len, keyword) != 0) {
      ++pos;
      continue;
    }

    const std::size_t val_start = scan + keyword_len;
    const int value = parse_int_suffix(line, val_start);
    const std::size_t val_end = numeric_suffix_end(line, val_start);
    if (val_end <= val_start) {
      ++pos;
      continue;
    }

    std::ostringstream payload;
    payload << "\"" << id_field << "\":" << index << ",\"" << value_field << "\":" << value;
    out.push_back(make_update(topic, event, payload_obj(payload.str())));
    pos = val_end;
  }
}

/**
 * Truncates leaked protocol markers from a parsed zone/source name.
 * @param name - Raw parsed name.
 * @returns Sanitized display name.
 */
/**
 * Detects names that are actually the next zone marker (e.g. 8NAME"Office").
 * @param name - Parsed candidate name.
 * @returns True when the value should be ignored.
 */
bool name_looks_like_leaked_marker(const std::string& name) {
  std::size_t i = 0;
  while (i < name.size() && std::isdigit(static_cast<unsigned char>(name[i]))) {
    ++i;
  }
  if (i > 0 && i + 4 <= name.size() && name.compare(i, 4, "NAME") == 0) {
    return true;
  }
  return name.find("NAME\"") != std::string::npos;
}

std::string sanitize_parsed_name(std::string name) {
  const auto hash = name.find('#');
  if (hash != std::string::npos) {
    name.erase(hash);
  }
  for (std::size_t i = 0; i + 1 < name.size(); ++i) {
    if (name[i] != 'Z') {
      continue;
    }
    std::size_t scan = i + 1;
    if (scan >= name.size() || !std::isdigit(static_cast<unsigned char>(name[scan]))) {
      continue;
    }
    while (scan < name.size() && std::isdigit(static_cast<unsigned char>(name[scan]))) {
      ++scan;
    }
    if (scan + 4 <= name.size() && name.compare(scan, 4, "NAME") == 0) {
      name.erase(i);
      break;
    }
  }
  while (!name.empty() && (name.back() == ' ' || name.back() == '\r')) {
    name.pop_back();
  }
  return name;
}

/**
 * Reads a name after the NAME keyword (quoted or unquoted until the next marker).
 * @param line - Full response line.
 * @param value_start - Index after the NAME keyword.
 * @param prefix - Z or S.
 * @returns Parsed name when present.
 */
std::optional<std::string> parse_name_value_after_keyword(const std::string& line,
                                                          std::size_t value_start, char prefix) {
  if (value_start >= line.size()) {
    return std::nullopt;
  }

  std::optional<std::string> raw;
  if (line[value_start] == '"') {
    raw = parse_quoted_value(line, value_start);
  } else {
    std::string out;
    for (std::size_t i = value_start; i < line.size(); ++i) {
      if (line[i] == '#') {
        break;
      }
      if (line[i] == prefix) {
        std::size_t scan = i + 1;
        if (scan < line.size() && std::isdigit(static_cast<unsigned char>(line[scan]))) {
          while (scan < line.size() && std::isdigit(static_cast<unsigned char>(line[scan]))) {
            ++scan;
          }
          if (scan + 4 <= line.size() && line.compare(scan, 4, "NAME") == 0) {
            break;
          }
        }
      }
      out.push_back(line[i]);
    }
    raw = out;
  }

  if (!raw) {
    return std::nullopt;
  }

  const std::string clean = sanitize_parsed_name(*raw);
  if (clean.empty()) {
    return std::nullopt;
  }
  return clean;
}

std::size_t quoted_value_end(const std::string& line, std::size_t quote_pos) {
  if (quote_pos >= line.size() || line[quote_pos] != '"') {
    return quote_pos;
  }
  for (std::size_t i = quote_pos + 1; i < line.size(); ++i) {
    if (line[i] == '\\' && i + 1 < line.size()) {
      ++i;
      continue;
    }
    if (line[i] == '"') {
      return i + 1;
    }
  }
  return line.size();
}

/**
 * Parses bulk ZzNAME"..." / SsNAME"..." name segments.
 * @param out - Parsed updates to append.
 * @param prefix - Z for zones, S for sources.
 * @param max_index - Highest zone/source index.
 * @param topic - Event topic.
 * @param event - Event name.
 * @param id_field - JSON id field (zone or source).
 * @param line - Full response line.
 * @param value_field - JSON value field name.
 */
void emit_prefixed_name_bulk(std::vector<ParsedUpdate>& out, char prefix, int max_index,
                             const std::string& topic, const std::string& event,
                             const char* id_field, const std::string& line,
                             const char* value_field = "name") {
  constexpr const char* kName = "NAME";
  constexpr std::size_t kNameLen = 4;

  std::size_t pos = 0;
  while (pos < line.size()) {
    if (line[pos] != prefix) {
      ++pos;
      continue;
    }

    const std::size_t digit_pos = pos + 1;
    if (digit_pos >= line.size()) {
      ++pos;
      continue;
    }

    int index = 0;
    std::size_t scan = digit_pos;
    if (line[scan] == '0') {
      index = 0;
      ++scan;
    } else if (std::isdigit(static_cast<unsigned char>(line[scan]))) {
      while (scan < line.size() && std::isdigit(static_cast<unsigned char>(line[scan]))) {
        index = index * 10 + (line[scan] - '0');
        ++scan;
      }
    } else {
      ++pos;
      continue;
    }

    if (index <= 0 || index > max_index) {
      ++pos;
      continue;
    }

    if (scan + kNameLen > line.size() || line.compare(scan, kNameLen, kName) != 0) {
      ++pos;
      continue;
    }

    const std::size_t value_start = scan + kNameLen;
    if (value_is_immediate_prefixed_keyword(line, value_start, prefix, max_index, kName)) {
      pos = value_start;
      continue;
    }

    const auto name = parse_name_value_after_keyword(line, value_start, prefix);
    if (!name) {
      ++pos;
      continue;
    }

    if (name_looks_like_leaked_marker(*name)) {
      pos = value_start;
      continue;
    }

    std::ostringstream payload;
    payload << "\"" << id_field << "\":" << index << ",\"" << value_field << "\":\""
            << json_escape(*name) << "\"";
    out.push_back(make_update(topic, event, payload_obj(payload.str())));

    if (value_start < line.size() && line[value_start] == '"') {
      const auto quote_pos = line.find('"', value_start);
      pos = quote_pos == std::string::npos ? value_start + 1 : quoted_value_end(line, quote_pos);
    } else {
      pos = value_start + name->size();
    }
  }
}

/**
 * Parses bulk Gz"..." name segments (groups omit the NAME keyword).
 * @param out - Parsed updates to append.
 * @param prefix - G for groups.
 * @param max_index - Highest group index.
 * @param topic - Event topic.
 * @param event - Event name.
 * @param id_field - JSON id field.
 * @param line - Full response line.
 * @param value_field - JSON value field name.
 */
void emit_indexed_quoted_names(std::vector<ParsedUpdate>& out, char prefix, int max_index,
                               const std::string& topic, const std::string& event,
                               const char* id_field, const std::string& line,
                               const char* value_field = "name") {
  std::vector<std::pair<std::size_t, std::size_t>> consumed;

  for (int i = max_index; i >= 1; --i) {
    const std::string marker = std::string(1, prefix) + std::to_string(i);
    std::size_t search_from = 0;
    while (search_from < line.size()) {
      const auto pos = line.find(marker, search_from);
      if (pos == std::string::npos) {
        break;
      }
      search_from = pos + 1;
      if (bulk_marker_shadowed(line, prefix, i, max_index, pos)) {
        continue;
      }

      const std::size_t value_start = pos + marker.size();
      if (value_start >= line.size() || line[value_start] != '"') {
        continue;
      }

      const auto name = parse_quoted_value(line, value_start);
      if (!name) {
        continue;
      }

      const std::size_t value_end = quoted_value_end(line, value_start);
      const bool overlaps = [&]() {
        for (const auto& span : consumed) {
          if (pos < span.second && value_end > span.first) {
            return true;
          }
        }
        return false;
      }();
      if (overlaps) {
        continue;
      }

      std::ostringstream payload;
      payload << "\"" << id_field << "\":" << i << ",\"" << value_field << "\":\""
              << json_escape(*name) << "\"";
      out.push_back(make_update(topic, event, payload_obj(payload.str())));
      consumed.push_back({pos, value_end});
      break;
    }
  }
}

}  // namespace

std::optional<std::string> parse_quoted_value(const std::string& line, std::size_t start) {
  const auto q = line.find('"', start);
  if (q == std::string::npos) {
    return std::nullopt;
  }
  std::string out;
  for (std::size_t i = q + 1; i < line.size(); ++i) {
    if (line[i] == '\\' && i + 1 < line.size()) {
      out.push_back(line[++i]);
      continue;
    }
    if (line[i] == '"') {
      return out;
    }
    out.push_back(line[i]);
  }
  return std::nullopt;
}

std::vector<ParsedUpdate> parse_response_line(const std::string& line) {
  std::vector<ParsedUpdate> updates;
  if (line.empty() || line[0] != '#') {
    return updates;
  }

  if (line.rfind("#?RECEIVED_COMMAND", 0) == 0) {
    updates.push_back(make_update("modules.audio.system", "protocol_error",
                                  payload_obj("\"message\":\"invalid command\"")));
    return updates;
  }

  if (line == "#ALLOFF") {
    updates.push_back(
        make_update("modules.audio.system", "all_zones_off", payload_obj("")));
    return updates;
  }

  if (line.rfind("#VER", 0) == 0) {
    const auto quoted = parse_quoted_value(line, 4);
    if (quoted) {
      updates.push_back(make_update("modules.audio.controller", "controller_version_changed",
                                    payload_obj("\"version\":\"" + json_escape(*quoted) + "\"")));
    }
    return updates;
  }

  if (line.rfind("#PAGE", 0) == 0) {
    const int page = parse_int_suffix(line, 5);
    updates.push_back(make_update("modules.audio.system", "page_state_changed",
                                  payload_obj("\"page\":" + std::to_string(page))));
    return updates;
  }

  if (line.rfind("#NETMAC", 0) == 0) {
    const auto mac = parse_quoted_value(line, 7);
    if (mac) {
      updates.push_back(make_update("modules.audio.controller", "network_config_changed",
                                    payload_obj("\"mac\":\"" + json_escape(*mac) + "\"")));
    }
    return updates;
  }

  if (line.rfind("#NETNAME", 0) == 0) {
    const auto name = parse_quoted_value(line, 8);
    if (name) {
      updates.push_back(make_update("modules.audio.controller", "network_config_changed",
                                    payload_obj("\"deviceName\":\"" + json_escape(*name) + "\"")));
    }
    return updates;
  }

  if (line.rfind("#NETDHCP", 0) == 0) {
    const int dhcp = parse_int_suffix(line, 8);
    updates.push_back(make_update("modules.audio.controller", "network_config_changed",
                                  payload_obj("\"dhcp\":" + std::to_string(dhcp))));
    return updates;
  }

  if (line.rfind("#NETPORT", 0) == 0) {
    const int port = parse_int_suffix(line, 8);
    updates.push_back(make_update("modules.audio.controller", "network_config_changed",
                                  payload_obj("\"tcpPort\":" + std::to_string(port))));
    return updates;
  }

  if (line.rfind("#NETIP", 0) == 0) {
    const auto quoted = parse_quoted_value(line, 6);
    if (quoted) {
      updates.push_back(make_update("modules.audio.controller", "network_config_changed",
                                    payload_obj("\"ipAddress\":\"" + json_escape(*quoted) + "\"")));
    } else {
      std::string ip = line.substr(6);
      while (!ip.empty() && (ip[0] == ' ' || ip[0] == '"')) {
        ip.erase(ip.begin());
      }
      if (!ip.empty()) {
        updates.push_back(make_update("modules.audio.controller", "network_config_changed",
                                      payload_obj("\"ipAddress\":\"" + json_escape(ip) + "\"")));
      }
    }
    return updates;
  }

  if (line.rfind("#NETMASK", 0) == 0) {
    const auto mask = parse_quoted_value(line, 8);
    if (mask) {
      updates.push_back(make_update("modules.audio.controller", "network_config_changed",
                                    payload_obj("\"subnetMask\":\"" + json_escape(*mask) + "\"")));
    }
    return updates;
  }

  if (line.rfind("#NETGATEWAY", 0) == 0) {
    const auto gateway = parse_quoted_value(line, 11);
    if (gateway) {
      updates.push_back(make_update("modules.audio.controller", "network_config_changed",
                                    payload_obj("\"gateway\":\"" + json_escape(*gateway) + "\"")));
    }
    return updates;
  }

  if (line.rfind("#LS", 0) == 0) {
    int idx = 0;
    std::size_t pos = 3;
    while (pos < line.size() && std::isdigit(static_cast<unsigned char>(line[pos]))) {
      idx = idx * 10 + (line[pos] - '0');
      ++pos;
    }
    const auto str = parse_quoted_value(line, line.find("STR", pos));
    if (str) {
      updates.push_back(make_update("modules.audio.controller", "language_string_changed",
                                    payload_obj("\"stringNumber\":" + std::to_string(idx) +
                                                ",\"value\":\"" + json_escape(*str) + "\"")));
    }
    return updates;
  }

  const auto zone = parse_zone_index(line, 1);
  if (zone) {
    const int z = *zone;
    if (line.find("NAME", 1) != std::string::npos) {
      if (z == 0) {
        emit_prefixed_name_bulk(updates, 'Z', 16, "modules.audio.zone", "zone_name_changed", "zone",
                                line);
      } else {
        const std::string marker = "Z" + std::to_string(z) + "NAME";
        const auto marker_pos = line.find(marker, 1);
        if (marker_pos != std::string::npos) {
          const auto name = parse_name_value_after_keyword(line, marker_pos + marker.size(), 'Z');
          if (name) {
            updates.push_back(make_update("modules.audio.zone", "zone_name_changed",
                                          payload_obj("\"zone\":" + std::to_string(z) +
                                                      ",\"name\":\"" + json_escape(*name) + "\"")));
          }
        }
      }
      return updates;
    }
    if (line.find("VOLUME", 1) != std::string::npos) {
      if (z == 0) {
        emit_prefixed_value_bulk(updates, 'Z', 16, "modules.audio.zone", "zone_volume_changed",
                                 "zone", "volume", "VOLUME", line);
      } else {
        const std::string marker = "Z" + std::to_string(z) + "VOLUME";
        const auto marker_pos = line.find(marker, 1);
        if (marker_pos != std::string::npos) {
          const int vol = parse_int_suffix(line, marker_pos + marker.size());
          updates.push_back(make_update("modules.audio.zone", "zone_volume_changed",
                                        payload_obj("\"zone\":" + std::to_string(z) +
                                                    ",\"volume\":" + std::to_string(vol))));
        }
      }
      return updates;
    }
    if (line.find("POWER", 1) != std::string::npos) {
      if (z == 0) {
        emit_zone_bulk(updates, "power", "zone_power_changed", line, 0);
      } else {
        const int power = parse_int_suffix(line, line.find("POWER") + 5);
        updates.push_back(make_update("modules.audio.zone", "zone_power_changed",
                                      payload_obj("\"zone\":" + std::to_string(z) +
                                                  ",\"power\":" + std::to_string(power))));
      }
      return updates;
    }
    if (line.find("ENABLE", 1) != std::string::npos) {
      if (z == 0) {
        emit_prefixed_enable_bulk(updates, 'Z', 16, "modules.audio.zone", "zone_enable_changed",
                                  "zone", line);
      } else {
        const std::string marker = "Z" + std::to_string(z) + "ENABLE";
        const auto marker_pos = line.find(marker, 1);
        if (marker_pos != std::string::npos) {
          const auto en = parse_enable_flag(line, marker_pos + marker.size() - 6);
          if (en) {
            updates.push_back(make_update("modules.audio.zone", "zone_enable_changed",
                                          payload_obj("\"zone\":" + std::to_string(z) +
                                                      ",\"enabled\":" + std::to_string(*en))));
          }
        }
      }
      return updates;
    }
    if (line.find("SRC", 1) != std::string::npos) {
      if (z == 0) {
        emit_zone_bulk(updates, "source", "zone_source_changed", line, 0);
      } else {
        const int src = parse_int_suffix(line, line.find("SRC") + 3);
        updates.push_back(make_update("modules.audio.zone", "zone_source_changed",
                                      payload_obj("\"zone\":" + std::to_string(z) +
                                                  ",\"source\":" + std::to_string(src))));
      }
      return updates;
    }
    if (line.find("MUTE", 1) != std::string::npos) {
      if (z == 0) {
        emit_zone_bulk(updates, "mute", "zone_mute_changed", line, 0);
      } else {
        const int mute = parse_int_suffix(line, line.find("MUTE") + 4);
        updates.push_back(make_update("modules.audio.zone", "zone_mute_changed",
                                      payload_obj("\"zone\":" + std::to_string(z) +
                                                  ",\"mute\":" + std::to_string(mute))));
      }
      return updates;
    }
    if (line.find("TREB", 1) != std::string::npos) {
      if (z == 0) {
        emit_zone_bulk(updates, "treble", "zone_treble_changed", line, 0);
      } else if (z > 0) {
        const int treble = parse_int_suffix(line, line.find("TREB") + 4);
        updates.push_back(make_update("modules.audio.zone", "zone_treble_changed",
                                      payload_obj("\"zone\":" + std::to_string(z) +
                                                  ",\"treble\":" + std::to_string(treble))));
      }
      return updates;
    }
    if (line.find("BASS", 1) != std::string::npos) {
      if (z == 0) {
        emit_zone_bulk(updates, "bass", "zone_bass_changed", line, 0);
      } else if (z > 0) {
        const int bass = parse_int_suffix(line, line.find("BASS") + 4);
        updates.push_back(make_update("modules.audio.zone", "zone_bass_changed",
                                      payload_obj("\"zone\":" + std::to_string(z) +
                                                  ",\"bass\":" + std::to_string(bass))));
      }
      return updates;
    }
    if (line.find("BAL", 1) != std::string::npos) {
      if (z == 0) {
        emit_zone_bulk(updates, "balance", "zone_balance_changed", line, 0);
      } else if (z > 0) {
        const int balance = parse_int_suffix(line, line.find("BAL") + 3);
        updates.push_back(make_update("modules.audio.zone", "zone_balance_changed",
                                      payload_obj("\"zone\":" + std::to_string(z) +
                                                  ",\"balance\":" + std::to_string(balance))));
      }
      return updates;
    }
    if (line.find("LOUDNESS", 1) != std::string::npos) {
      if (z == 0) {
        emit_zone_bulk(updates, "loudness", "zone_loudness_changed", line, 0);
      } else if (z > 0) {
        const int loudness = parse_int_suffix(line, line.find("LOUDNESS") + 8);
        updates.push_back(make_update("modules.audio.zone", "zone_loudness_changed",
                                      payload_obj("\"zone\":" + std::to_string(z) +
                                                  ",\"loudness\":" + std::to_string(loudness))));
      }
      return updates;
    }
    if (line.find("INIVOL", 1) != std::string::npos) {
      if (z == 0) {
        emit_prefixed_value_bulk(updates, 'Z', 16, "modules.audio.zone", "zone_initial_volume_changed",
                                 "zone", "initialVolume", "INIVOL", line);
      } else if (z > 0) {
        const int vol = parse_int_suffix(line, line.find("INIVOL") + 6);
        updates.push_back(
            make_update("modules.audio.zone", "zone_initial_volume_changed",
                        payload_obj("\"zone\":" + std::to_string(z) +
                                    ",\"initialVolume\":" + std::to_string(vol))));
      }
      return updates;
    }
    if (line.find("PGVOL", 1) != std::string::npos) {
      if (z == 0) {
        emit_prefixed_value_bulk(updates, 'Z', 16, "modules.audio.zone", "zone_page_volume_changed",
                                 "zone", "pageVolume", "PGVOL", line);
      } else if (z > 0) {
        const int vol = parse_int_suffix(line, line.find("PGVOL") + 5);
        updates.push_back(make_update("modules.audio.zone", "zone_page_volume_changed",
                                      payload_obj("\"zone\":" + std::to_string(z) +
                                                  ",\"pageVolume\":" + std::to_string(vol))));
      }
      return updates;
    }
    if (line.find("GROUP", 1) != std::string::npos) {
      if (z == 0) {
        emit_zone_bulk(updates, "groupNumber", "zone_group_changed", line, 0);
      } else if (z > 0) {
        const int group = parse_int_suffix(line, line.find("GROUP") + 5);
        updates.push_back(make_update("modules.audio.zone", "zone_group_changed",
                                      payload_obj("\"zone\":" + std::to_string(z) +
                                                  ",\"groupNumber\":" + std::to_string(group))));
      }
      return updates;
    }
  }

  const auto source = parse_source_index(line, 1);
  if (source) {
    const int s = *source;
    if (line.find("NAME", 1) != std::string::npos) {
      if (s == 0) {
        emit_prefixed_name_bulk(updates, 'S', 8, "modules.audio.source", "source_name_changed",
                                "source", line);
      } else {
        const std::string marker = "S" + std::to_string(s) + "NAME";
        const auto marker_pos = line.find(marker, 1);
        if (marker_pos != std::string::npos) {
          const auto name = parse_name_value_after_keyword(line, marker_pos + marker.size(), 'S');
          if (name) {
            updates.push_back(make_update("modules.audio.source", "source_name_changed",
                                          payload_obj("\"source\":" + std::to_string(s) +
                                                      ",\"name\":\"" + json_escape(*name) + "\"")));
          }
        }
      }
      return updates;
    }
    if (line.find("ENABLE", 1) != std::string::npos) {
      if (s == 0) {
        emit_prefixed_enable_bulk(updates, 'S', 8, "modules.audio.source", "source_enable_changed",
                                  "source", line);
      } else {
        const auto enable_pos = line.find("ENABLE", 1);
        const auto en = parse_enable_flag(line, enable_pos);
        if (en) {
          updates.push_back(make_update("modules.audio.source", "source_enable_changed",
                                        payload_obj("\"source\":" + std::to_string(s) +
                                                    ",\"enabled\":" + std::to_string(*en))));
        }
      }
      return updates;
    }
    if (line.find("INGAIN", 1) != std::string::npos) {
      if (s == 0) {
        emit_indexed_numeric_bulk(updates, 'S', 8, "modules.audio.source", "source_input_gain_changed",
                                  "source", "inputGain", line);
      } else {
        const int gain = parse_int_suffix(line, line.find("INGAIN") + 6);
        updates.push_back(make_update("modules.audio.source", "source_input_gain_changed",
                                      payload_obj("\"source\":" + std::to_string(s) +
                                                  ",\"inputGain\":" + std::to_string(gain))));
      }
      return updates;
    }
    if (line.find("DISPLINE", 1) != std::string::npos) {
      if (s == 0) {
        emit_indexed_quoted_names(updates, 'S', 8, "modules.audio.source", "source_display_line_changed",
                                  "source", line, "displayLine");
      } else {
        const auto display = parse_quoted_value(line, line.find("DISPLINE") + 8);
        if (display) {
          updates.push_back(
              make_update("modules.audio.source", "source_display_line_changed",
                          payload_obj("\"source\":" + std::to_string(s) + ",\"displayLine\":\"" +
                                      json_escape(*display) + "\"")));
        }
      }
      return updates;
    }
  }

  const auto group = parse_group_index(line, 1);
  if (group) {
    const int g = *group;
    if (line.find("NAME", 1) != std::string::npos) {
      if (g == 0) {
        emit_indexed_quoted_names(updates, 'G', 8, "modules.audio.group", "group_name_changed", "group",
                                  line);
      } else {
        const auto name = parse_quoted_value(line, line.find("NAME") + 4);
        if (name) {
          updates.push_back(make_update("modules.audio.group", "group_name_changed",
                                        payload_obj("\"group\":" + std::to_string(g) + ",\"name\":\"" +
                                                    json_escape(*name) + "\"")));
        }
      }
      return updates;
    }
    if (line.find("TYPE", 1) != std::string::npos) {
      if (g == 0) {
        emit_indexed_numeric_bulk(updates, 'G', 8, "modules.audio.group", "group_type_changed", "group",
                                  "type", line);
      } else {
        const int type = parse_int_suffix(line, line.find("TYPE") + 4);
        updates.push_back(make_update("modules.audio.group", "group_type_changed",
                                      payload_obj("\"group\":" + std::to_string(g) +
                                                  ",\"type\":" + std::to_string(type))));
      }
      return updates;
    }
  }

  updates.push_back(make_update("modules.audio.system", "protocol_response",
                                payload_obj("\"raw\":\"" + json_escape(line) + "\"")));
  return updates;
}

}  // namespace homepi::hifi_serial
