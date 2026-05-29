#include "dac_resolver.h"

#include <sqlite3.h>
#include <stdio.h>
#include <string.h>

#include "log.h"

bool dac_resolver_load_primary(const HomepiConfig* cfg, DacAssignment* out) {
  if (!cfg || !out) {
    return false;
  }
  memset(out, 0, sizeof(*out));

  sqlite3* db = NULL;
  if (sqlite3_open_v2(cfg->database_path, &db, SQLITE_OPEN_READONLY, NULL) != SQLITE_OK) {
    log_msg(LOG_LEVEL_ERROR, "dac_resolver", "db_open_failed", cfg->database_path);
    return false;
  }

  const char* sql =
      "SELECT d.device_id, d.display_name, d.kind, d.present "
      "FROM usb_assignments a "
      "JOIN usb_devices d ON d.device_id = a.audio_primary_device_id "
      "WHERE a.id = 1;";

  sqlite3_stmt* stmt = NULL;
  bool ok = false;
  if (sqlite3_prepare_v2(db, sql, -1, &stmt, NULL) != SQLITE_OK) {
    log_msg(LOG_LEVEL_ERROR, "dac_resolver", "prepare_failed", sqlite3_errmsg(db));
    sqlite3_close(db);
    return false;
  }

  if (sqlite3_step(stmt) == SQLITE_ROW) {
    const char* device_id = (const char*)sqlite3_column_text(stmt, 0);
    const char* display_name = (const char*)sqlite3_column_text(stmt, 1);
    const char* kind = (const char*)sqlite3_column_text(stmt, 2);
    const int present = sqlite3_column_int(stmt, 3);

    if (device_id) {
      snprintf(out->device_id, sizeof(out->device_id), "%s", device_id);
    }
    if (display_name) {
      snprintf(out->display_name, sizeof(out->display_name), "%s", display_name);
    }
    out->present = present != 0;
    config_primary_dac_device(cfg, out->dac_device, sizeof(out->dac_device));

    if (!kind || strcmp(kind, "audio") != 0) {
      log_msg(LOG_LEVEL_ERROR, "dac_resolver", "not_audio_device", out->device_id);
    } else if (!out->present) {
      log_msg(LOG_LEVEL_ERROR, "dac_resolver", "device_not_present", out->device_id);
    } else if (out->dac_device[0] == '\0') {
      log_msg(LOG_LEVEL_ERROR, "dac_resolver", "empty_dac_device", "primary");
    } else {
      ok = true;
    }
  } else {
    log_msg(LOG_LEVEL_ERROR, "dac_resolver", "no_primary_assignment",
            "Set Primary Audio Output in USB Devices settings");
  }

  sqlite3_finalize(stmt);
  sqlite3_close(db);
  return ok;
}
