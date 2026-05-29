#include "homepi/hifi-serial/protocol-parser.hpp"

#include <cctype>
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

void emit_indexed_numeric_bulk(std::vector<ParsedUpdate>& out, char prefix, int max_index,
                               const std::string& topic, const std::string& event,
                               const char* id_field, const char* value_field,
                               const std::string& line) {
  for (int i = max_index; i >= 1; --i) {
    const std::string marker = std::string(1, prefix) + std::to_string(i);
    const auto pos = line.find(marker);
    if (pos == std::string::npos) {
      continue;
    }
    const int value = parse_int_suffix(line, pos + marker.size());
    std::ostringstream payload;
    payload << "\"" << id_field << "\":" << i << ",\"" << value_field << "\":" << value;
    out.push_back(make_update(topic, event, payload_obj(payload.str())));
  }
}

void emit_zone_bulk(std::vector<ParsedUpdate>& out, const std::string& field, const std::string& event,
                    const std::string& line, std::size_t /*prefix_len*/) {
  emit_indexed_numeric_bulk(out, 'Z', 16, "modules.audio.zone", event, "zone", field.c_str(), line);
}

void emit_indexed_quoted_names(std::vector<ParsedUpdate>& out, char prefix, int max_index,
                               const std::string& topic, const std::string& event,
                               const char* id_field, const std::string& line,
                               const char* value_field = "name") {
  for (int i = max_index; i >= 1; --i) {
    const std::string marker = std::string(1, prefix) + std::to_string(i);
    const auto pos = line.find(marker);
    if (pos == std::string::npos) {
      continue;
    }
    const auto name = parse_quoted_value(line, pos + marker.size());
    if (name) {
      std::ostringstream payload;
      payload << "\"" << id_field << "\":" << i << ",\"" << value_field << "\":\""
              << json_escape(*name) << "\"";
      out.push_back(make_update(topic, event, payload_obj(payload.str())));
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
        emit_indexed_quoted_names(updates, 'Z', 16, "modules.audio.zone", "zone_name_changed", "zone",
                                  line);
      } else {
        const auto name = parse_quoted_value(line, line.find("NAME") + 4);
        if (name) {
          updates.push_back(make_update("modules.audio.zone", "zone_name_changed",
                                        payload_obj("\"zone\":" + std::to_string(z) + ",\"name\":\"" +
                                                    json_escape(*name) + "\"")));
        }
      }
      return updates;
    }
    if (line.find("VOLUME", 1) != std::string::npos) {
      if (z == 0) {
        emit_zone_bulk(updates, "volume", "zone_volume_changed", line, 0);
      } else {
        const int vol = parse_int_suffix(line, line.find("VOLUME") + 6);
        updates.push_back(make_update("modules.audio.zone", "zone_volume_changed",
                                      payload_obj("\"zone\":" + std::to_string(z) +
                                                  ",\"volume\":" + std::to_string(vol))));
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
        emit_zone_bulk(updates, "enabled", "zone_enable_changed", line, 0);
      } else {
        const int en = parse_int_suffix(line, line.find("ENABLE") + 6);
        updates.push_back(make_update("modules.audio.zone", "zone_enable_changed",
                                      payload_obj("\"zone\":" + std::to_string(z) +
                                                  ",\"enabled\":" + std::to_string(en))));
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
        emit_zone_bulk(updates, "initialVolume", "zone_initial_volume_changed", line, 0);
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
        emit_zone_bulk(updates, "pageVolume", "zone_page_volume_changed", line, 0);
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
        emit_indexed_quoted_names(updates, 'S', 8, "modules.audio.source", "source_name_changed",
                                  "source", line);
      } else {
        const auto name = parse_quoted_value(line, line.find("NAME") + 4);
        if (name) {
          updates.push_back(make_update("modules.audio.source", "source_name_changed",
                                        payload_obj("\"source\":" + std::to_string(s) +
                                                    ",\"name\":\"" + json_escape(*name) + "\"")));
        }
      }
      return updates;
    }
    if (line.find("ENABLE", 1) != std::string::npos) {
      if (s == 0) {
        emit_indexed_numeric_bulk(updates, 'S', 8, "modules.audio.source", "source_enable_changed",
                                  "source", "enabled", line);
      } else {
        const int en = parse_int_suffix(line, line.find("ENABLE") + 6);
        updates.push_back(make_update("modules.audio.source", "source_enable_changed",
                                      payload_obj("\"source\":" + std::to_string(s) +
                                                  ",\"enabled\":" + std::to_string(en))));
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
