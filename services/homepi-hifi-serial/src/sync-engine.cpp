#include "homepi/hifi-serial/sync-engine.hpp"

#include "homepi/hifi-serial/protocol-encoder.hpp"

namespace homepi::hifi_serial {

void SyncEngine::run_full_sync(CommandQueue& queue) {
  queue.enqueue(cmd_ver_query());
  queue.enqueue(cmd_netconfig_query());
  queue.enqueue(cmd_page_query());

  queue.enqueue(cmd_zone_name_query(0));
  queue.enqueue(cmd_zone_enable_query(0));
  queue.enqueue(cmd_zone_treb_query(0));
  queue.enqueue(cmd_zone_bass_query(0));
  queue.enqueue(cmd_zone_bal_query(0));
  queue.enqueue(cmd_zone_loudness_query(0));
  queue.enqueue(cmd_zone_inivol_query(0));
  queue.enqueue(cmd_zone_pgvol_query(0));
  queue.enqueue(cmd_zone_group_query(0));
  queue.enqueue(cmd_zone_power_query(0));
  queue.enqueue(cmd_zone_volume_query(0));
  queue.enqueue(cmd_zone_mute_query(0));
  queue.enqueue(cmd_zone_src_query(0));

  for (int source = 1; source <= 8; ++source) {
    queue.enqueue(cmd_source_name_query(source));
  }
  queue.enqueue(cmd_source_enable_query(0));
  queue.enqueue(cmd_source_ingain_query(0));
  queue.enqueue(cmd_source_displine_query(0));

  queue.enqueue(cmd_group_name_query(0));
  queue.enqueue(cmd_group_type_query(0));

  for (int i = 0; i <= 100; ++i) {
    queue.enqueue(cmd_language_string_query(i));
  }
}

}  // namespace homepi::hifi_serial
