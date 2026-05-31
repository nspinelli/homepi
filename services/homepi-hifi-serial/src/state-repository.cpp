#include "homepi/hifi-serial/state-repository.hpp"

#include <chrono>
#include <ctime>
#include <sqlite3.h>
#include <sstream>
#include <stdexcept>

#include "homepi/hifi-serial/json-utils.hpp"

namespace homepi::hifi_serial {

namespace {

std::string utc_now() {
  const auto now = std::chrono::system_clock::now();
  const std::time_t t = std::chrono::system_clock::to_time_t(now);
  std::tm tm{};
  gmtime_r(&t, &tm);
  char buffer[32];
  std::strftime(buffer, sizeof(buffer), "%Y-%m-%dT%H:%M:%SZ", &tm);
  return buffer;
}

int json_get_int(const std::string& json, const char* field) {
  const std::string key = "\"" + std::string(field) + "\"";
  const auto pos = json.find(key);
  if (pos == std::string::npos) {
    return -1;
  }
  const auto colon = json.find(':', pos);
  if (colon == std::string::npos) {
    return -1;
  }
  return std::stoi(json.substr(colon + 1));
}

std::string sql_escape(const std::string& value) {
  std::string out;
  out.reserve(value.size());
  for (char ch : value) {
    if (ch == '\'') {
      out += "''";
    } else {
      out.push_back(ch);
    }
  }
  return out;
}

void upsert_zone_int(sqlite3* db, int zone, const char* column, int value, const std::string& now) {
  if (zone <= 0) {
    return;
  }
  const std::string sql =
      "INSERT INTO hifi_zones(zone_number," + std::string(column) +
      ",updated_at) VALUES(" + std::to_string(zone) + "," + std::to_string(value) + ",'" + now +
      "') ON CONFLICT(zone_number) DO UPDATE SET " + column +
      "=excluded." + column + ",updated_at=excluded.updated_at";
  sqlite3_exec(db, sql.c_str(), nullptr, nullptr, nullptr);
}

void upsert_zone_text(sqlite3* db, int zone, const char* column, const std::string& value,
                      const std::string& now) {
  if (zone <= 0) {
    return;
  }
  const std::string sql =
      "INSERT INTO hifi_zones(zone_number," + std::string(column) +
      ",updated_at) VALUES(" + std::to_string(zone) + ",'" + sql_escape(value) + "','" + now +
      "') ON CONFLICT(zone_number) DO UPDATE SET " + column +
      "=excluded." + column + ",updated_at=excluded.updated_at";
  sqlite3_exec(db, sql.c_str(), nullptr, nullptr, nullptr);
}

void upsert_source_int(sqlite3* db, int source, const char* column, int value,
                       const std::string& now) {
  if (source <= 0) {
    return;
  }
  const std::string sql =
      "INSERT INTO hifi_sources(source_number," + std::string(column) +
      ",updated_at) VALUES(" + std::to_string(source) + "," + std::to_string(value) + ",'" + now +
      "') ON CONFLICT(source_number) DO UPDATE SET " + column +
      "=excluded." + column + ",updated_at=excluded.updated_at";
  sqlite3_exec(db, sql.c_str(), nullptr, nullptr, nullptr);
}

void upsert_source_text(sqlite3* db, int source, const char* column, const std::string& value,
                        const std::string& now) {
  if (source <= 0) {
    return;
  }
  const std::string sql =
      "INSERT INTO hifi_sources(source_number," + std::string(column) +
      ",updated_at) VALUES(" + std::to_string(source) + ",'" + sql_escape(value) + "','" + now +
      "') ON CONFLICT(source_number) DO UPDATE SET " + column +
      "=excluded." + column + ",updated_at=excluded.updated_at";
  sqlite3_exec(db, sql.c_str(), nullptr, nullptr, nullptr);
}

void upsert_group_int(sqlite3* db, int group, const char* column, int value, const std::string& now) {
  if (group <= 0) {
    return;
  }
  const std::string sql =
      "INSERT INTO hifi_groups(group_number," + std::string(column) +
      ",updated_at) VALUES(" + std::to_string(group) + "," + std::to_string(value) + ",'" + now +
      "') ON CONFLICT(group_number) DO UPDATE SET " + column +
      "=excluded." + column + ",updated_at=excluded.updated_at";
  sqlite3_exec(db, sql.c_str(), nullptr, nullptr, nullptr);
}

void upsert_group_text(sqlite3* db, int group, const char* column, const std::string& value,
                       const std::string& now) {
  if (group <= 0) {
    return;
  }
  const std::string sql =
      "INSERT INTO hifi_groups(group_number," + std::string(column) +
      ",updated_at) VALUES(" + std::to_string(group) + ",'" + sql_escape(value) + "','" + now +
      "') ON CONFLICT(group_number) DO UPDATE SET " + column +
      "=excluded." + column + ",updated_at=excluded.updated_at";
  sqlite3_exec(db, sql.c_str(), nullptr, nullptr, nullptr);
}

void update_controller_text(sqlite3* db, const char* column, const std::string& value,
                          const std::string& now) {
  const std::string sql = "UPDATE hifi_controller SET " + std::string(column) + "='" +
                          sql_escape(value) + "',updated_at='" + now + "' WHERE id=1";
  sqlite3_exec(db, sql.c_str(), nullptr, nullptr, nullptr);
}

void update_controller_int(sqlite3* db, const char* column, int value, const std::string& now) {
  const std::string sql = "UPDATE hifi_controller SET " + std::string(column) + "=" +
                          std::to_string(value) + ",updated_at='" + now + "' WHERE id=1";
  sqlite3_exec(db, sql.c_str(), nullptr, nullptr, nullptr);
}

void apply_migration_sqlite(sqlite3* db, const std::string& migration_sql) {
  char* err = nullptr;
  if (sqlite3_exec(db, migration_sql.c_str(), nullptr, nullptr, &err) != SQLITE_OK) {
    const std::string message = err != nullptr ? err : "migration failed";
    sqlite3_free(err);
    if (message.find("duplicate column name") == std::string::npos) {
      throw std::runtime_error(message);
    }
  }
}

}  // namespace

StateRepository::StateRepository(const std::string& database_path,
                               const std::string& migration_sql) {
  sqlite3* raw = nullptr;
  if (sqlite3_open(database_path.c_str(), &raw) != SQLITE_OK) {
    throw std::runtime_error("Failed to open database: " + database_path);
  }
  db_ = raw;
  apply_migration(migration_sql);
}

void StateRepository::apply_migration(const std::string& migration_sql) {
  apply_migration_sqlite(static_cast<sqlite3*>(db_), migration_sql);
}

StateRepository::~StateRepository() {
  if (db_ != nullptr) {
    sqlite3_close(static_cast<sqlite3*>(db_));
    db_ = nullptr;
  }
}

void StateRepository::patch_zone_controller(int zone_number, const std::string& fields_json) {
  if (zone_number < 1 || zone_number > 16) {
    return;
  }
  auto* db = static_cast<sqlite3*>(db_);
  const std::string now = utc_now();
  const std::string& p = fields_json;

  if (p.find("\"enabled\"") != std::string::npos) {
    upsert_zone_int(db, zone_number, "enabled", json_get_int(p, "enabled"), now);
  }
  if (p.find("\"name\"") != std::string::npos) {
    upsert_zone_text(db, zone_number, "name", json_get_string(p, "name"), now);
  }
  if (p.find("\"treble\"") != std::string::npos) {
    upsert_zone_int(db, zone_number, "treble", json_get_int(p, "treble"), now);
  }
  if (p.find("\"bass\"") != std::string::npos) {
    upsert_zone_int(db, zone_number, "bass", json_get_int(p, "bass"), now);
  }
  if (p.find("\"balance\"") != std::string::npos) {
    upsert_zone_int(db, zone_number, "balance", json_get_int(p, "balance"), now);
  }
  if (p.find("\"loudness\"") != std::string::npos) {
    upsert_zone_int(db, zone_number, "loudness", json_get_int(p, "loudness"), now);
  }
  if (p.find("\"initialVolume\"") != std::string::npos) {
    const int initial_volume = json_get_int(p, "initialVolume");
    if (initial_volume >= 0 && initial_volume <= 100) {
      upsert_zone_int(db, zone_number, "initial_volume", initial_volume, now);
    }
  }
  if (p.find("\"pageVolume\"") != std::string::npos) {
    upsert_zone_int(db, zone_number, "page_volume", json_get_int(p, "pageVolume"), now);
  }
  if (p.find("\"groupNumber\"") != std::string::npos) {
    upsert_zone_int(db, zone_number, "group_number", json_get_int(p, "groupNumber"), now);
  }
  if (p.find("\"power\"") != std::string::npos) {
    upsert_zone_int(db, zone_number, "power", json_get_int(p, "power"), now);
  }
  if (p.find("\"volume\"") != std::string::npos) {
    upsert_zone_int(db, zone_number, "volume", json_get_int(p, "volume"), now);
  }
}

void StateRepository::apply_parsed_update(const ParsedUpdate& update) {
  auto* db = static_cast<sqlite3*>(db_);
  const std::string now = utc_now();
  const std::string& p = update.payload_json;

  if (update.event_name == "controller_version_changed") {
    const std::string ver = json_get_string(p, "version");
    update_controller_text(db, "firmware_version", ver, now);
    const auto hw_pos = ver.find("HWv");
    if (hw_pos != std::string::npos) {
      std::size_t end = ver.find(' ', hw_pos);
      update_controller_text(db, "hardware_version",
                             ver.substr(hw_pos, end == std::string::npos ? std::string::npos : end - hw_pos),
                             now);
    }
    return;
  }

  if (update.event_name == "network_config_changed") {
    const std::string mac = json_get_string(p, "mac");
    const std::string device_name = json_get_string(p, "deviceName");
    const std::string ip = json_get_string(p, "ipAddress");
    const std::string mask = json_get_string(p, "subnetMask");
    const std::string gateway = json_get_string(p, "gateway");
    if (!mac.empty()) {
      update_controller_text(db, "mac_address", mac, now);
    }
    if (!device_name.empty()) {
      update_controller_text(db, "device_name", device_name, now);
    }
    if (!ip.empty()) {
      update_controller_text(db, "ip_address", ip, now);
    }
    if (!mask.empty()) {
      update_controller_text(db, "subnet_mask", mask, now);
    }
    if (!gateway.empty()) {
      update_controller_text(db, "gateway", gateway, now);
    }
    if (json_get_int(p, "dhcp") >= 0) {
      update_controller_int(db, "dhcp_enabled", json_get_int(p, "dhcp"), now);
    }
    if (json_get_int(p, "tcpPort") > 0) {
      update_controller_int(db, "tcp_port", json_get_int(p, "tcpPort"), now);
    }
    return;
  }

  if (update.event_name == "page_state_changed") {
    update_controller_int(db, "page_active", json_get_int(p, "page"), now);
    return;
  }

  if (update.event_name == "language_string_changed") {
    const int index = json_get_int(p, "stringNumber");
    const std::string value = json_get_string(p, "value");
    if (index >= 0) {
      const std::string sql =
          "INSERT INTO hifi_language_strings(string_number,value,updated_at) VALUES(" +
          std::to_string(index) + ",'" + sql_escape(value) + "','" + now +
          "') ON CONFLICT(string_number) DO UPDATE SET value=excluded.value,updated_at=excluded.updated_at";
      sqlite3_exec(db, sql.c_str(), nullptr, nullptr, nullptr);
    }
    return;
  }

  if (update.event_name == "zone_volume_changed") {
    upsert_zone_int(db, json_get_int(p, "zone"), "volume", json_get_int(p, "volume"), now);
    return;
  }
  if (update.event_name == "zone_power_changed") {
    upsert_zone_int(db, json_get_int(p, "zone"), "power", json_get_int(p, "power"), now);
    return;
  }
  if (update.event_name == "zone_name_changed") {
    upsert_zone_text(db, json_get_int(p, "zone"), "name", json_get_string(p, "name"), now);
    return;
  }
  if (update.event_name == "zone_source_changed") {
    upsert_zone_int(db, json_get_int(p, "zone"), "source", json_get_int(p, "source"), now);
    return;
  }
  if (update.event_name == "zone_enable_changed") {
    upsert_zone_int(db, json_get_int(p, "zone"), "enabled", json_get_int(p, "enabled"), now);
    return;
  }
  if (update.event_name == "zone_mute_changed") {
    upsert_zone_int(db, json_get_int(p, "zone"), "mute", json_get_int(p, "mute"), now);
    return;
  }
  if (update.event_name == "zone_treble_changed") {
    upsert_zone_int(db, json_get_int(p, "zone"), "treble", json_get_int(p, "treble"), now);
    return;
  }
  if (update.event_name == "zone_bass_changed") {
    upsert_zone_int(db, json_get_int(p, "zone"), "bass", json_get_int(p, "bass"), now);
    return;
  }
  if (update.event_name == "zone_balance_changed") {
    upsert_zone_int(db, json_get_int(p, "zone"), "balance", json_get_int(p, "balance"), now);
    return;
  }
  if (update.event_name == "zone_loudness_changed") {
    upsert_zone_int(db, json_get_int(p, "zone"), "loudness", json_get_int(p, "loudness"), now);
    return;
  }
  if (update.event_name == "zone_initial_volume_changed") {
    upsert_zone_int(db, json_get_int(p, "zone"), "initial_volume", json_get_int(p, "initialVolume"),
                    now);
    return;
  }
  if (update.event_name == "zone_page_volume_changed") {
    upsert_zone_int(db, json_get_int(p, "zone"), "page_volume", json_get_int(p, "pageVolume"), now);
    return;
  }
  if (update.event_name == "zone_group_changed") {
    upsert_zone_int(db, json_get_int(p, "zone"), "group_number", json_get_int(p, "groupNumber"), now);
    return;
  }

  if (update.event_name == "source_name_changed") {
    upsert_source_text(db, json_get_int(p, "source"), "name", json_get_string(p, "name"), now);
    return;
  }
  if (update.event_name == "source_enable_changed") {
    upsert_source_int(db, json_get_int(p, "source"), "enabled", json_get_int(p, "enabled"), now);
    return;
  }
  if (update.event_name == "source_input_gain_changed") {
    upsert_source_int(db, json_get_int(p, "source"), "input_gain", json_get_int(p, "inputGain"), now);
    return;
  }
  if (update.event_name == "source_display_line_changed") {
    std::string display_line = json_get_string(p, "displayLine");
    if (display_line.empty()) {
      display_line = json_get_string(p, "name");
    }
    upsert_source_text(db, json_get_int(p, "source"), "display_line", display_line, now);
    return;
  }

  if (update.event_name == "group_name_changed") {
    upsert_group_text(db, json_get_int(p, "group"), "name", json_get_string(p, "name"), now);
    return;
  }
  if (update.event_name == "group_type_changed") {
    upsert_group_int(db, json_get_int(p, "group"), "type", json_get_int(p, "type"), now);
    return;
  }
}

void StateRepository::set_serial_metadata(const std::string& device_id, const std::string& path) {
  const std::string now = utc_now();
  auto* db = static_cast<sqlite3*>(db_);
  const std::string sql =
      "UPDATE hifi_controller SET serial_device_id='" + device_id + "',serial_path='" + path +
      "',updated_at='" + now + "' WHERE id=1";
  sqlite3_exec(db, sql.c_str(), nullptr, nullptr, nullptr);
}

void StateRepository::mark_full_sync_complete() {
  const std::string now = utc_now();
  sqlite3_exec(static_cast<sqlite3*>(db_),
               ("UPDATE hifi_controller SET last_full_sync_at='" + now + "',updated_at='" + now +
                "' WHERE id=1")
                   .c_str(),
               nullptr, nullptr, nullptr);
}

ControllerState StateRepository::get_controller() const {
  ControllerState state;
  state.updated_at = utc_now();
  return state;
}

std::vector<ZoneState> StateRepository::get_zones() const {
  std::vector<ZoneState> zones;
  sqlite3_stmt* stmt = nullptr;
  const char* sql = "SELECT zone_number,name,enabled,treble,bass,balance,loudness,initial_volume,"
                    "page_volume,group_number,power,volume,mute,source,updated_at FROM hifi_zones "
                    "ORDER BY zone_number";
  auto* db = static_cast<sqlite3*>(db_);
  if (sqlite3_prepare_v2(db, sql, -1, &stmt, nullptr) != SQLITE_OK) {
    return zones;
  }
  while (sqlite3_step(stmt) == SQLITE_ROW) {
    ZoneState z;
    z.zone_number = sqlite3_column_int(stmt, 0);
    if (sqlite3_column_type(stmt, 1) != SQLITE_NULL) {
      z.name = reinterpret_cast<const char*>(sqlite3_column_text(stmt, 1));
    }
    if (sqlite3_column_type(stmt, 2) != SQLITE_NULL) {
      z.enabled = sqlite3_column_int(stmt, 2);
    }
    if (sqlite3_column_type(stmt, 3) != SQLITE_NULL) {
      z.treble = sqlite3_column_int(stmt, 3);
    }
    if (sqlite3_column_type(stmt, 4) != SQLITE_NULL) {
      z.bass = sqlite3_column_int(stmt, 4);
    }
    if (sqlite3_column_type(stmt, 5) != SQLITE_NULL) {
      z.balance = sqlite3_column_int(stmt, 5);
    }
    if (sqlite3_column_type(stmt, 6) != SQLITE_NULL) {
      z.loudness = sqlite3_column_int(stmt, 6);
    }
    if (sqlite3_column_type(stmt, 7) != SQLITE_NULL) {
      z.initial_volume = sqlite3_column_int(stmt, 7);
    }
    if (sqlite3_column_type(stmt, 8) != SQLITE_NULL) {
      z.page_volume = sqlite3_column_int(stmt, 8);
    }
    if (sqlite3_column_type(stmt, 9) != SQLITE_NULL) {
      z.group_number = sqlite3_column_int(stmt, 9);
    }
    if (sqlite3_column_type(stmt, 10) != SQLITE_NULL) {
      z.power = sqlite3_column_int(stmt, 10);
    }
    if (sqlite3_column_type(stmt, 11) != SQLITE_NULL) {
      z.volume = sqlite3_column_int(stmt, 11);
    }
    if (sqlite3_column_type(stmt, 12) != SQLITE_NULL) {
      z.mute = sqlite3_column_int(stmt, 12);
    }
    if (sqlite3_column_type(stmt, 13) != SQLITE_NULL) {
      z.source = sqlite3_column_int(stmt, 13);
    }
    if (sqlite3_column_type(stmt, 14) != SQLITE_NULL) {
      z.updated_at = reinterpret_cast<const char*>(sqlite3_column_text(stmt, 14));
    }
    zones.push_back(z);
  }
  sqlite3_finalize(stmt);
  return zones;
}

std::vector<SourceState> StateRepository::get_sources() const {
  std::vector<SourceState> sources;
  sqlite3_stmt* stmt = nullptr;
  const char* sql =
      "SELECT source_number,name,enabled,input_gain,display_line,is_airplay,updated_at FROM hifi_sources "
      "ORDER BY source_number";
  auto* db = static_cast<sqlite3*>(db_);
  if (sqlite3_prepare_v2(db, sql, -1, &stmt, nullptr) != SQLITE_OK) {
    return sources;
  }
  while (sqlite3_step(stmt) == SQLITE_ROW) {
    SourceState s;
    s.source_number = sqlite3_column_int(stmt, 0);
    if (sqlite3_column_type(stmt, 1) != SQLITE_NULL) {
      s.name = reinterpret_cast<const char*>(sqlite3_column_text(stmt, 1));
    }
    if (sqlite3_column_type(stmt, 2) != SQLITE_NULL) {
      s.enabled = sqlite3_column_int(stmt, 2);
    }
    if (sqlite3_column_type(stmt, 3) != SQLITE_NULL) {
      s.input_gain = sqlite3_column_int(stmt, 3);
    }
    if (sqlite3_column_type(stmt, 4) != SQLITE_NULL) {
      s.display_line = reinterpret_cast<const char*>(sqlite3_column_text(stmt, 4));
    }
    if (sqlite3_column_type(stmt, 5) != SQLITE_NULL) {
      s.is_airplay = sqlite3_column_int(stmt, 5);
    }
    sources.push_back(s);
  }
  sqlite3_finalize(stmt);
  return sources;
}

std::vector<GroupState> StateRepository::get_groups() const {
  std::vector<GroupState> groups;
  sqlite3_stmt* stmt = nullptr;
  const char* sql = "SELECT group_number,name,type,updated_at FROM hifi_groups ORDER BY group_number";
  auto* db = static_cast<sqlite3*>(db_);
  if (sqlite3_prepare_v2(db, sql, -1, &stmt, nullptr) != SQLITE_OK) {
    return groups;
  }
  while (sqlite3_step(stmt) == SQLITE_ROW) {
    GroupState g;
    g.group_number = sqlite3_column_int(stmt, 0);
    if (sqlite3_column_type(stmt, 1) != SQLITE_NULL) {
      g.name = reinterpret_cast<const char*>(sqlite3_column_text(stmt, 1));
    }
    if (sqlite3_column_type(stmt, 2) != SQLITE_NULL) {
      g.type = sqlite3_column_int(stmt, 2);
    }
    groups.push_back(g);
  }
  sqlite3_finalize(stmt);
  return groups;
}

std::vector<LanguageStringState> StateRepository::get_language_strings() const {
  std::vector<LanguageStringState> strings;
  sqlite3_stmt* stmt = nullptr;
  const char* sql =
      "SELECT string_number,value,updated_at FROM hifi_language_strings ORDER BY string_number";
  auto* db = static_cast<sqlite3*>(db_);
  if (sqlite3_prepare_v2(db, sql, -1, &stmt, nullptr) != SQLITE_OK) {
    return strings;
  }
  while (sqlite3_step(stmt) == SQLITE_ROW) {
    LanguageStringState entry;
    entry.string_number = sqlite3_column_int(stmt, 0);
    if (sqlite3_column_type(stmt, 1) != SQLITE_NULL) {
      entry.value = reinterpret_cast<const char*>(sqlite3_column_text(stmt, 1));
    }
    strings.push_back(entry);
  }
  sqlite3_finalize(stmt);
  return strings;
}

std::string StateRepository::zones_json() const {
  std::ostringstream out;
  out << "[";
  const auto zones = get_zones();
  for (std::size_t i = 0; i < zones.size(); ++i) {
    if (i > 0) {
      out << ",";
    }
    const auto& z = zones[i];
    out << "{\"zoneNumber\":" << z.zone_number;
    if (z.name) {
      out << ",\"name\":\"" << json_escape(*z.name) << "\"";
    }
    out << ",\"enabled\":" << (z.enabled.has_value() ? *z.enabled : 0);
    if (z.treble) {
      out << ",\"treble\":" << *z.treble;
    }
    if (z.bass) {
      out << ",\"bass\":" << *z.bass;
    }
    if (z.balance) {
      out << ",\"balance\":" << *z.balance;
    }
    if (z.loudness) {
      out << ",\"loudness\":" << *z.loudness;
    }
    if (z.initial_volume.has_value()) {
      out << ",\"initialVolume\":" << *z.initial_volume;
    }
    if (z.page_volume.has_value()) {
      out << ",\"pageVolume\":" << *z.page_volume;
    }
    if (z.group_number) {
      out << ",\"groupNumber\":" << *z.group_number;
    }
    if (z.power) {
      out << ",\"power\":" << *z.power;
    }
    if (z.volume) {
      out << ",\"volume\":" << *z.volume;
    }
    if (z.mute) {
      out << ",\"mute\":" << *z.mute;
    }
    if (z.source) {
      out << ",\"source\":" << *z.source;
    }
    out << "}";
  }
  out << "]";
  return out.str();
}

std::string StateRepository::sources_json() const {
  std::ostringstream out;
  out << "[";
  const auto sources = get_sources();
  for (std::size_t i = 0; i < sources.size(); ++i) {
    if (i > 0) {
      out << ",";
    }
    out << "{\"sourceNumber\":" << sources[i].source_number;
    if (sources[i].name) {
      out << ",\"name\":\"" << json_escape(*sources[i].name) << "\"";
    }
    if (sources[i].enabled) {
      out << ",\"enabled\":" << *sources[i].enabled;
    }
    if (sources[i].input_gain) {
      out << ",\"inputGain\":" << *sources[i].input_gain;
    }
    if (sources[i].is_airplay) {
      out << ",\"isAirplay\":" << *sources[i].is_airplay;
    }
    out << "}";
  }
  out << "]";
  return out.str();
}

std::string StateRepository::groups_json() const {
  std::ostringstream out;
  out << "[";
  const auto groups = get_groups();
  for (std::size_t i = 0; i < groups.size(); ++i) {
    if (i > 0) {
      out << ",";
    }
    out << "{\"groupNumber\":" << groups[i].group_number;
    if (groups[i].name) {
      out << ",\"name\":\"" << json_escape(*groups[i].name) << "\"";
    }
    if (groups[i].type) {
      out << ",\"type\":" << *groups[i].type;
    }
    out << "}";
  }
  out << "]";
  return out.str();
}

std::string StateRepository::controller_json() const {
  sqlite3_stmt* stmt = nullptr;
  const char* sql =
      "SELECT firmware_version,hardware_version,device_name,mac_address,dhcp_enabled,ip_address,"
      "subnet_mask,gateway,tcp_port,page_active,serial_device_id,serial_path,last_full_sync_at,"
      "updated_at FROM hifi_controller WHERE id=1";
  auto* db = static_cast<sqlite3*>(db_);
  if (sqlite3_prepare_v2(db, sql, -1, &stmt, nullptr) != SQLITE_OK) {
    return "{}";
  }
  if (sqlite3_step(stmt) != SQLITE_ROW) {
    sqlite3_finalize(stmt);
    return "{}";
  }

  std::ostringstream out;
  out << "{";
  auto col_text = [&](int col, const char* key) {
    if (sqlite3_column_type(stmt, col) != SQLITE_NULL) {
      out << "\"" << key << "\":\""
          << json_escape(reinterpret_cast<const char*>(sqlite3_column_text(stmt, col))) << "\",";
    }
  };
  auto col_int = [&](int col, const char* key) {
    if (sqlite3_column_type(stmt, col) != SQLITE_NULL) {
      out << "\"" << key << "\":" << sqlite3_column_int(stmt, col) << ",";
    }
  };

  col_text(0, "firmwareVersion");
  col_text(1, "hardwareVersion");
  col_text(2, "deviceName");
  col_text(3, "macAddress");
  col_int(4, "dhcpEnabled");
  col_text(5, "ipAddress");
  col_text(6, "subnetMask");
  col_text(7, "gateway");
  col_int(8, "tcpPort");
  col_int(9, "pageActive");
  col_text(10, "serialDeviceId");
  col_text(11, "serialPath");
  col_text(12, "lastFullSyncAt");

  std::string json = out.str();
  if (!json.empty() && json.back() == ',') {
    json.pop_back();
  }
  json += "}";
  sqlite3_finalize(stmt);
  return json;
}

std::string StateRepository::snapshot_json() const {
  std::ostringstream out;
  out << "{\"controller\":" << controller_json() << ",\"zones\":" << zones_json()
      << ",\"sources\":" << sources_json() << ",\"groups\":" << groups_json() << "}";
  return out.str();
}

std::optional<int> StateRepository::airplay_source_number() const {
  sqlite3_stmt* stmt = nullptr;
  const char* sql = "SELECT source_number FROM hifi_sources WHERE is_airplay = 1 LIMIT 2";
  auto* db = static_cast<sqlite3*>(db_);
  if (sqlite3_prepare_v2(db, sql, -1, &stmt, nullptr) != SQLITE_OK) {
    return std::nullopt;
  }
  std::optional<int> source;
  int count = 0;
  while (sqlite3_step(stmt) == SQLITE_ROW) {
    source = sqlite3_column_int(stmt, 0);
    ++count;
  }
  sqlite3_finalize(stmt);
  if (count != 1) {
    return std::nullopt;
  }
  return source;
}

void StateRepository::set_airplay_source(int source_number) {
  if (source_number < 1 || source_number > 8) {
    return;
  }
  auto* db = static_cast<sqlite3*>(db_);
  const std::string now = utc_now();
  sqlite3_exec(db, "UPDATE hifi_sources SET is_airplay = 0", nullptr, nullptr, nullptr);
  const std::string sql =
      "INSERT INTO hifi_sources(source_number,is_airplay,updated_at) VALUES(" +
      std::to_string(source_number) + ",1,'" + now +
      "') ON CONFLICT(source_number) DO UPDATE SET is_airplay=1,updated_at=excluded.updated_at";
  sqlite3_exec(db, sql.c_str(), nullptr, nullptr, nullptr);
}

std::string StateRepository::shairport_zone_settings_json() const {
  std::ostringstream out;
  out << "[";
  sqlite3_stmt* stmt = nullptr;
  const char* sql =
      "SELECT zone_number,volume_control_profile,active_state_timeout,session_timeout,"
      "log_verbosity FROM shairport_zone_settings ORDER BY zone_number";
  auto* db = static_cast<sqlite3*>(db_);
  if (sqlite3_prepare_v2(db, sql, -1, &stmt, nullptr) != SQLITE_OK) {
    return "[]";
  }
  bool first = true;
  while (sqlite3_step(stmt) == SQLITE_ROW) {
    if (!first) {
      out << ",";
    }
    first = false;
    out << "{\"zoneNumber\":" << sqlite3_column_int(stmt, 0);
    if (sqlite3_column_type(stmt, 1) != SQLITE_NULL) {
      out << ",\"volumeControlProfile\":\""
          << json_escape(reinterpret_cast<const char*>(sqlite3_column_text(stmt, 1))) << "\"";
    }
    out << ",\"activeStateTimeout\":" << sqlite3_column_double(stmt, 2);
    out << ",\"sessionTimeout\":" << sqlite3_column_int(stmt, 3);
    out << ",\"logVerbosity\":" << sqlite3_column_int(stmt, 4);
    out << "}";
  }
  sqlite3_finalize(stmt);
  out << "]";
  return out.str();
}

void StateRepository::update_shairport_zone_settings(int zone_number,
                                                     const std::string& volume_control_profile,
                                                     double active_state_timeout,
                                                     int session_timeout, int log_verbosity) {
  if (zone_number < 1 || zone_number > 16) {
    return;
  }
  auto* db = static_cast<sqlite3*>(db_);
  const std::string now = utc_now();
  std::ostringstream sql;
  sql << "UPDATE shairport_zone_settings SET updated_at='" << now << "'";
  if (!volume_control_profile.empty()) {
    sql << ",volume_control_profile='" << sql_escape(volume_control_profile) << "'";
  }
  if (active_state_timeout >= 0) {
    sql << ",active_state_timeout=" << active_state_timeout;
  }
  if (session_timeout >= 0) {
    sql << ",session_timeout=" << session_timeout;
  }
  if (log_verbosity >= 0) {
    sql << ",log_verbosity=" << log_verbosity;
  }
  sql << " WHERE zone_number=" << zone_number;
  sqlite3_exec(db, sql.str().c_str(), nullptr, nullptr, nullptr);
}

}  // namespace homepi::hifi_serial
