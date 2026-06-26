#include "homepi/hifi-serial/command-dispatcher.hpp"

#include <vector>

#include "homepi/hifi-serial/json-utils.hpp"
#include "homepi/hifi-serial/protocol-encoder.hpp"

namespace homepi::hifi_serial {

namespace {

int zone_from_payload(const std::string& payload_json) {
  const int zone_number = json_get_int(payload_json, "zoneNumber");
  if (zone_number > 0) {
    return zone_number;
  }
  return json_get_int(payload_json, "zoneId");
}

int source_from_payload(const std::string& payload_json) {
  return json_get_int(payload_json, "sourceNumber");
}

int power_from_payload(const std::string& payload_json) {
  if (json_get_bool(payload_json, "power")) {
    return 1;
  }
  const int power = json_get_int(payload_json, "power");
  return power >= 0 ? power : 0;
}

std::vector<std::string> build_zone_controller_patch(int zone, const std::string& payload_json) {
  std::vector<std::string> commands;
  const std::string name = json_get_string(payload_json, "name");
  if (!name.empty()) {
    commands.push_back(cmd_zone_name_set(zone, name));
  }
  const int enabled = json_get_int(payload_json, "enabled");
  if (enabled >= 0) {
    commands.push_back(cmd_zone_enable_set(zone, enabled));
  }
  const int power = json_get_int(payload_json, "power");
  if (power >= 0) {
    commands.push_back(cmd_zone_power_set(zone, power));
  }
  const int volume = json_get_int(payload_json, "volume");
  if (volume >= 0) {
    commands.push_back(cmd_zone_volume_set(zone, volume));
  }
  const int treble = json_get_int(payload_json, "treble");
  if (treble >= 0) {
    commands.push_back(cmd_zone_treb_set(zone, treble));
  }
  const int bass = json_get_int(payload_json, "bass");
  if (bass >= 0) {
    commands.push_back(cmd_zone_bass_set(zone, bass));
  }
  const int balance = json_get_int(payload_json, "balance");
  if (balance >= 0) {
    commands.push_back(cmd_zone_bal_set(zone, balance));
  }
  const int loudness = json_get_int(payload_json, "loudness");
  if (loudness >= 0) {
    commands.push_back(cmd_zone_loudness_set(zone, loudness));
  }
  const int initial_volume = json_get_int(payload_json, "initialVolume");
  if (initial_volume >= 0) {
    commands.push_back(cmd_zone_inivol_set(zone, initial_volume));
  }
  const int page_volume = json_get_int(payload_json, "pageVolume");
  if (page_volume >= 0) {
    commands.push_back(cmd_zone_pgvol_set(zone, page_volume));
  }
  const int group_number = json_get_int(payload_json, "groupNumber");
  if (group_number >= 0) {
    commands.push_back(cmd_zone_group_set(zone, group_number));
  }
  return commands;
}

std::vector<std::string> build_source_patch(int source, const std::string& payload_json) {
  std::vector<std::string> commands;
  const std::string name = json_get_string(payload_json, "name");
  if (!name.empty()) {
    commands.push_back(cmd_source_name_set(source, name));
  }
  const int enabled = json_get_int(payload_json, "enabled");
  if (enabled >= 0) {
    commands.push_back(cmd_source_enable_set(source, enabled));
  }
  const int input_gain = json_get_int(payload_json, "inputGain");
  if (input_gain >= 0) {
    commands.push_back(cmd_source_ingain_set(source, input_gain));
  }
  const std::string display_line = json_get_string(payload_json, "displayLine");
  if (!display_line.empty()) {
    commands.push_back(cmd_source_displine_set(source, display_line));
  }
  return commands;
}

}  // namespace

CommandDispatcher::CommandDispatcher(CommandQueue& queue, CommandStatusFn on_status)
    : queue_(queue), on_status_(std::move(on_status)) {}

void CommandDispatcher::enqueue_commands(const std::vector<std::string>& commands,
                                         const std::string& event,
                                         const std::string& correlation_id) {
  for (const std::string& command : commands) {
    queue_.enqueue(command);
  }
  if (!commands.empty() && on_status_) {
    on_status_(event, correlation_id, static_cast<int>(commands.size()));
  }
}

int CommandDispatcher::dispatch(const std::string& event, const std::string& payload_json,
                                const std::string& correlation_id) {
  std::vector<std::string> commands;

  if (event == "set_zone_power_source") {
    const int zone = zone_from_payload(payload_json);
    if (zone < 1 || zone > 16) {
      return 0;
    }
    commands.push_back(cmd_zone_power_set(zone, power_from_payload(payload_json)));
    const int source = source_from_payload(payload_json);
    if (source > 0) {
      commands.push_back(cmd_zone_src_set(zone, source));
    }
  } else if (event == "set_zone_power") {
    const int zone = zone_from_payload(payload_json);
    if (zone >= 1 && zone <= 16) {
      commands.push_back(cmd_zone_power_set(zone, power_from_payload(payload_json)));
    }
  } else if (event == "set_zone_source") {
    const int zone = zone_from_payload(payload_json);
    const int source = source_from_payload(payload_json);
    if (zone >= 1 && zone <= 16 && source > 0) {
      commands.push_back(cmd_zone_src_set(zone, source));
    }
  } else if (event == "set_zone_volume") {
    const int zone = zone_from_payload(payload_json);
    const int volume = json_get_int(payload_json, "volume");
    if (zone >= 1 && zone <= 16 && volume >= 0) {
      commands.push_back(cmd_zone_volume_set(zone, volume));
    }
  } else if (event == "set_zone_enabled" || event == "set_zone_enable") {
    const int zone = zone_from_payload(payload_json);
    const int enabled = json_get_int(payload_json, "enabled");
    if (zone >= 1 && zone <= 16 && enabled >= 0) {
      commands.push_back(cmd_zone_enable_set(zone, enabled));
    }
  } else if (event == "apply_zone_controller_patch") {
    const int zone = zone_from_payload(payload_json);
    if (zone >= 1 && zone <= 16) {
      commands = build_zone_controller_patch(zone, payload_json);
    }
  } else if (event == "apply_source_patch") {
    const int source = source_from_payload(payload_json);
    if (source >= 1 && source <= 8) {
      commands = build_source_patch(source, payload_json);
    }
  } else if (event == "set_controller_netname") {
    const std::string name = json_get_string(payload_json, "deviceName");
    if (!name.empty()) {
      commands.push_back(cmd_netname_set(name));
    }
  } else if (event == "page_start") {
    commands.push_back(cmd_page_on());
  } else if (event == "page_end") {
    commands.push_back(cmd_page_off());
  }

  enqueue_commands(commands, event, correlation_id);
  return static_cast<int>(commands.size());
}

}  // namespace homepi::hifi_serial
