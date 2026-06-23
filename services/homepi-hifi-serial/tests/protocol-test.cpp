#include <cassert>
#include <iostream>

#include "homepi/hifi-serial/protocol-encoder.hpp"
#include "homepi/hifi-serial/protocol-parser.hpp"

int main() {
  using namespace homepi::hifi_serial;

  const std::string ver_cmd = cmd_ver_query();
  assert(ver_cmd == "*VER\r");

  assert(cmd_zone_power_set(3, 1) == "*Z3POWER1\r");
  assert(cmd_zone_volume_set(4, 50) == "*Z4VOLUME50\r");
  assert(cmd_zone_src_set(2, 7) == "*Z2SRC7\r");

  const auto updates = parse_response_line("#Z4VOLUME50");
  assert(!updates.empty());
  assert(updates[0].event_name == "zone_volume_changed");

  const auto ver_updates = parse_response_line("#VER\"HAIHIFI2 FWv1.01 HWv0\"");
  assert(!ver_updates.empty());
  assert(ver_updates[0].event_name == "controller_version_changed");

  const auto source_names = parse_response_line("#S1NAME\"Radio\"");
  assert(source_names.size() == 1);
  assert(source_names[0].event_name == "source_name_changed");

  const auto group_names = parse_response_line("#G0NAMEG1\"GROUP 1\"G2\"GROUP 2\"");
  assert(group_names.size() == 2);
  assert(group_names[0].event_name == "group_name_changed");

  const auto zone_enable_single = parse_response_line("#Z9ENABLE0");
  assert(zone_enable_single.size() == 1);
  assert(zone_enable_single[0].event_name == "zone_enable_changed");
  assert(zone_enable_single[0].payload_json.find("\"zone\":9,\"enabled\":0") != std::string::npos);

  const auto zone_enable_two_digit = parse_response_line("#Z10ENABLE0");
  assert(zone_enable_two_digit.size() == 1);
  assert(zone_enable_two_digit[0].payload_json.find("\"zone\":10,\"enabled\":0") != std::string::npos);

  const auto source_enable_single = parse_response_line("#S2ENABLE1");
  assert(source_enable_single.size() == 1);
  assert(source_enable_single[0].event_name == "source_enable_changed");
  assert(source_enable_single[0].payload_json.find("\"source\":2,\"enabled\":1") != std::string::npos);

  const auto source_enable_bulk = parse_response_line("#S0ENABLES1ENABLE0S2ENABLE1");
  assert(source_enable_bulk.size() == 2);
  for (const auto& update : source_enable_bulk) {
    assert(update.event_name == "source_enable_changed");
  }

  const auto zone_inivol_single = parse_response_line("#Z7INIVOL35");
  assert(zone_inivol_single.size() == 1);
  assert(zone_inivol_single[0].event_name == "zone_initial_volume_changed");
  assert(zone_inivol_single[0].payload_json.find("\"zone\":7,\"initialVolume\":35") !=
         std::string::npos);

  const auto zone_inivol_bulk = parse_response_line("#Z0INIVOLZ7INIVOL35Z8INIVOL24");
  assert(zone_inivol_bulk.size() == 2);
  bool found_z7_inivol = false;
  for (const auto& update : zone_inivol_bulk) {
    assert(update.event_name == "zone_initial_volume_changed");
    if (update.payload_json.find("\"zone\":7,\"initialVolume\":35") != std::string::npos) {
      found_z7_inivol = true;
    }
  }
  assert(found_z7_inivol);

  const auto zone_name_single = parse_response_line("#Z8NAME\"Office\"");
  assert(zone_name_single.size() == 1);
  assert(zone_name_single[0].event_name == "zone_name_changed");
  assert(zone_name_single[0].payload_json.find("\"zone\":8,\"name\":\"Office\"") !=
         std::string::npos);

  const auto zone_name_bulk = parse_response_line(
      "#Z0NAMEZ4NAME\"Living Room\"Z5NAME\"Front Porch\"Z8NAME\"Office\"Z9NAME\"Zone 9\"");
  assert(zone_name_bulk.size() == 4);
  bool found_office = false;
  bool found_front_porch = false;
  for (const auto& update : zone_name_bulk) {
    assert(update.event_name == "zone_name_changed");
    if (update.payload_json.find("\"zone\":8,\"name\":\"Office\"") != std::string::npos) {
      found_office = true;
    }
    if (update.payload_json.find("\"zone\":5,\"name\":\"Front Porch\"") != std::string::npos) {
      found_front_porch = true;
    }
    if (update.payload_json.find("7NAME") != std::string::npos) {
      assert(false);
    }
  }
  assert(found_office);
  assert(found_front_porch);

  const auto zone_name_no_shadow = parse_response_line("#Z0NAMEZ50NAME\"Zone Fifty\"Z5NAME\"Front Porch\"");
  assert(zone_name_no_shadow.size() == 1);
  assert(zone_name_no_shadow[0].payload_json.find("\"zone\":5,\"name\":\"Front Porch\"") !=
         std::string::npos);

  const auto zone_name_sanitize = parse_response_line("#Z0NAMEZ9NAME\"ZONE 9#Z10NAME\"ZONE 10\"");
  assert(zone_name_sanitize.size() == 1);
  assert(zone_name_sanitize[0].payload_json.find("\"zone\":9,\"name\":\"ZONE 9\"") !=
         std::string::npos);

  const auto zone_name_empty_slot =
      parse_response_line("#Z0NAMEZ7NAMEZ8NAME\"Office\"Z2NAME\"Playroom\"");
  assert(zone_name_empty_slot.size() == 2);
  bool found_playroom = false;
  bool found_office_empty_slot = false;
  for (const auto& update : zone_name_empty_slot) {
    assert(update.event_name == "zone_name_changed");
    if (update.payload_json.find("\"zone\":2,\"name\":\"Playroom\"") != std::string::npos) {
      found_playroom = true;
    }
    if (update.payload_json.find("\"zone\":8,\"name\":\"Office\"") != std::string::npos) {
      found_office_empty_slot = true;
    }
    if (update.payload_json.find("\"zone\":7") != std::string::npos) {
      assert(false);
    }
  }
  assert(found_playroom);
  assert(found_office_empty_slot);

  const auto zone_enable_shadow =
      parse_response_line("#Z0ENABLEZ17ENABLE0Z8ENABLE1Z7ENABLE1");
  assert(zone_enable_shadow.size() == 2);
  bool found_z8_enabled_shadow = false;
  bool found_z7_enabled_shadow = false;
  for (const auto& update : zone_enable_shadow) {
    assert(update.event_name == "zone_enable_changed");
    if (update.payload_json.find("\"zone\":8,\"enabled\":1") != std::string::npos) {
      found_z8_enabled_shadow = true;
    }
    if (update.payload_json.find("\"zone\":7,\"enabled\":1") != std::string::npos) {
      found_z7_enabled_shadow = true;
    }
    if (update.payload_json.find("\"zone\":17") != std::string::npos) {
      assert(false);
    }
  }
  assert(found_z8_enabled_shadow);
  assert(found_z7_enabled_shadow);

  const auto zone_volume_single = parse_response_line("#Z7VOLUME55");
  assert(zone_volume_single.size() == 1);
  assert(zone_volume_single[0].event_name == "zone_volume_changed");
  assert(zone_volume_single[0].payload_json.find("\"zone\":7,\"volume\":55") != std::string::npos);

  const auto zone_volume_bulk = parse_response_line("#Z0VOLUMEZ5VOLUME35Z8VOLUME60");
  assert(zone_volume_bulk.size() == 2);
  bool found_z8_volume = false;
  for (const auto& update : zone_volume_bulk) {
    assert(update.event_name == "zone_volume_changed");
    if (update.payload_json.find("\"zone\":8,\"volume\":60") != std::string::npos) {
      found_z8_volume = true;
    }
  }
  assert(found_z8_volume);

  const auto zone_enable_bulk = parse_response_line("#Z0ENABLEZ9ENABLE0Z10ENABLE0Z8ENABLE1");
  assert(zone_enable_bulk.size() == 3);
  bool found_z10_disabled = false;
  bool found_z9_disabled = false;
  bool found_z8_enabled = false;
  for (const auto& update : zone_enable_bulk) {
    assert(update.event_name == "zone_enable_changed");
    if (update.payload_json.find("\"zone\":10,\"enabled\":0") != std::string::npos) {
      found_z10_disabled = true;
    }
    if (update.payload_json.find("\"zone\":9,\"enabled\":0") != std::string::npos) {
      found_z9_disabled = true;
    }
    if (update.payload_json.find("\"zone\":8,\"enabled\":1") != std::string::npos) {
      found_z8_enabled = true;
    }
  }
  assert(found_z10_disabled);
  assert(found_z9_disabled);
  assert(found_z8_enabled);

  const auto netip_bulk =
      parse_response_line("#NETIP\"192.168.1.100\",\"255.255.255.0\",\"192.168.1.1\"");
  assert(netip_bulk.size() == 1);
  assert(netip_bulk[0].event_name == "network_config_changed");
  assert(netip_bulk[0].payload_json.find("\"ipAddress\":\"192.168.1.100\"") != std::string::npos);
  assert(netip_bulk[0].payload_json.find("\"subnetMask\":\"255.255.255.0\"") != std::string::npos);
  assert(netip_bulk[0].payload_json.find("\"gateway\":\"192.168.1.1\"") != std::string::npos);

  const auto netip_single = parse_response_line("#NETIP\"10.0.0.5\"");
  assert(netip_single.size() == 1);
  assert(netip_single[0].payload_json.find("\"ipAddress\":\"10.0.0.5\"") != std::string::npos);
  assert(netip_single[0].payload_json.find("subnetMask") == std::string::npos);

  assert(cmd_source_name_set(3, "Radio") == "*S3NAME\"Radio\"\r");
  assert(cmd_source_name_set(1, "Line \"A\"") == "*S1NAME\"Line \\\"A\\\"\"\r");
  assert(cmd_source_enable_set(2, 1) == "*S2ENABLE1\r");
  assert(cmd_source_ingain_set(4, 12) == "*S4INGAIN12\r");
  assert(cmd_source_displine_set(5, "Now Playing") == "*S5DISPLINE\"Now Playing\"\r");

  std::cout << "protocol tests passed\n";
  return 0;
}
