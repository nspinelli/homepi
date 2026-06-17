#include "homepi/hifi-serial/protocol-encoder.hpp"

namespace homepi::hifi_serial {

std::string escape_protocol_string(const std::string& value) {
  std::string out;
  out.reserve(value.size());
  for (char ch : value) {
    if (ch == '"' || ch == '*' || ch == '\\') {
      out.push_back('\\');
    }
    out.push_back(ch);
  }
  return out;
}

std::string build_command(const std::string& body) {
  return "*" + body + "\r";
}

std::string cmd_ver_query() { return build_command("VER"); }

std::string cmd_zone_name_query(int zone) {
  return build_command("Z" + std::to_string(zone) + "NAME?");
}

std::string cmd_zone_volume_query(int zone) {
  return build_command("Z" + std::to_string(zone) + "VOLUME?");
}

std::string cmd_zone_power_query(int zone) {
  return build_command("Z" + std::to_string(zone) + "POWER?");
}

std::string cmd_zone_enable_query(int zone) {
  return build_command("Z" + std::to_string(zone) + "ENABLE?");
}

std::string cmd_zone_treb_query(int zone) {
  return build_command("Z" + std::to_string(zone) + "TREB?");
}

std::string cmd_zone_bass_query(int zone) {
  return build_command("Z" + std::to_string(zone) + "BASS?");
}

std::string cmd_zone_bal_query(int zone) {
  return build_command("Z" + std::to_string(zone) + "BAL?");
}

std::string cmd_zone_loudness_query(int zone) {
  return build_command("Z" + std::to_string(zone) + "LOUDNESS?");
}

std::string cmd_zone_inivol_query(int zone) {
  return build_command("Z" + std::to_string(zone) + "INIVOL?");
}

std::string cmd_zone_pgvol_query(int zone) {
  return build_command("Z" + std::to_string(zone) + "PGVOL?");
}

std::string cmd_zone_group_query(int zone) {
  return build_command("Z" + std::to_string(zone) + "GROUP?");
}

std::string cmd_zone_mute_query(int zone) {
  return build_command("Z" + std::to_string(zone) + "MUTE?");
}

std::string cmd_zone_src_query(int zone) {
  return build_command("Z" + std::to_string(zone) + "SRC?");
}

std::string cmd_source_name_query(int source) {
  return build_command("S" + std::to_string(source) + "NAME?");
}

std::string cmd_source_enable_query(int source) {
  return build_command("S" + std::to_string(source) + "ENABLE?");
}

std::string cmd_source_ingain_query(int source) {
  return build_command("S" + std::to_string(source) + "INGAIN?");
}

std::string cmd_source_displine_query(int source) {
  return build_command("S" + std::to_string(source) + "DISPLINE?");
}

std::string cmd_group_name_query(int group) {
  return build_command("G" + std::to_string(group) + "NAME?");
}

std::string cmd_group_type_query(int group) {
  return build_command("G" + std::to_string(group) + "TYPE?");
}

std::string cmd_netconfig_query() { return build_command("NETCONFIG?"); }

std::string cmd_page_query() { return build_command("PAGE?"); }

std::string cmd_language_string_query(int index) {
  return build_command("LS" + std::to_string(index) + "STR?");
}

std::string cmd_raw(const std::string& command) {
  if (!command.empty() && command[0] == '*') {
    if (command.back() == '\r') {
      return command;
    }
    return command + "\r";
  }
  return build_command(command);
}

std::string cmd_zone_name_set(int zone, const std::string& name) {
  return build_command("Z" + std::to_string(zone) + "NAME\"" + escape_protocol_string(name) + "\"");
}

std::string cmd_zone_enable_set(int zone, int enabled) {
  return build_command("Z" + std::to_string(zone) + "ENABLE" + std::to_string(enabled));
}

std::string cmd_zone_treb_set(int zone, int treble) {
  return build_command("Z" + std::to_string(zone) + "TREB" + std::to_string(treble));
}

std::string cmd_zone_bass_set(int zone, int bass) {
  return build_command("Z" + std::to_string(zone) + "BASS" + std::to_string(bass));
}

std::string cmd_zone_bal_set(int zone, int balance) {
  return build_command("Z" + std::to_string(zone) + "BAL" + std::to_string(balance));
}

std::string cmd_zone_loudness_set(int zone, int loudness) {
  return build_command("Z" + std::to_string(zone) + "LOUDNESS" + std::to_string(loudness));
}

std::string cmd_zone_inivol_set(int zone, int volume) {
  return build_command("Z" + std::to_string(zone) + "INIVOL" + std::to_string(volume));
}

std::string cmd_zone_pgvol_set(int zone, int volume) {
  return build_command("Z" + std::to_string(zone) + "PGVOL" + std::to_string(volume));
}

std::string cmd_zone_group_set(int zone, int group) {
  return build_command("Z" + std::to_string(zone) + "GROUP" + std::to_string(group));
}

std::string cmd_source_name_set(int source, const std::string& name) {
  return build_command("S" + std::to_string(source) + "NAME\"" + escape_protocol_string(name) +
                       "\"");
}

std::string cmd_source_enable_set(int source, int enabled) {
  return build_command("S" + std::to_string(source) + "ENABLE" + std::to_string(enabled));
}

std::string cmd_source_ingain_set(int source, int gain) {
  return build_command("S" + std::to_string(source) + "INGAIN" + std::to_string(gain));
}

std::string cmd_source_displine_set(int source, const std::string& line) {
  return build_command("S" + std::to_string(source) + "DISPLINE\"" +
                       escape_protocol_string(line) + "\"");
}

}  // namespace homepi::hifi_serial
