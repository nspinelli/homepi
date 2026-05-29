#include <cassert>
#include <iostream>

#include "homepi/hifi-serial/protocol-encoder.hpp"
#include "homepi/hifi-serial/protocol-parser.hpp"

int main() {
  using namespace homepi::hifi_serial;

  const std::string ver_cmd = cmd_ver_query();
  assert(ver_cmd == "*VER\r");

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

  const auto source_enable = parse_response_line("#S0ENABLES10S21");
  assert(source_enable.size() == 2);
  assert(source_enable[0].event_name == "source_enable_changed");

  std::cout << "protocol tests passed\n";
  return 0;
}
