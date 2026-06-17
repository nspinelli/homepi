#include "homepi/shairport-sync/config-generator.hpp"

#include <array>
#include <cstdio>
#include <filesystem>
#include <fstream>
#include <iomanip>
#include <sstream>

namespace fs = std::filesystem;

namespace homepi::shairport_sync {

namespace {

std::string alsa_output_device(int zone) {
  if (zone >= 1 && zone <= 8) {
    return "hw:HomePiZonesA,0," + std::to_string(zone - 1);
  }
  return "hw:HomePiZonesB,0," + std::to_string(zone - 9);
}

double inivol_to_apple_db(int inivol) {
  if (inivol < 0) {
    inivol = 0;
  }
  if (inivol > 100) {
    inivol = 100;
  }
  return -30.0 + (static_cast<double>(inivol) / 100.0) * 30.0;
}

std::string hook_path(const ServiceConfig& config, const std::string& name) {
  return config.hooks_dir + "/" + name;
}

void write_executable(const fs::path& path, const std::string& content) {
  fs::create_directories(path.parent_path());
  const fs::path temp = path.string() + ".tmp";
  {
    std::ofstream out(temp);
    out << content;
  }
  fs::permissions(temp, fs::perms::owner_all | fs::perms::group_read | fs::perms::group_exec |
                            fs::perms::others_read | fs::perms::others_exec);
  fs::rename(temp, path);
}

void write_file(const fs::path& path, const std::string& content) {
  fs::create_directories(path.parent_path());
  const fs::path temp = path.string() + ".tmp";
  {
    std::ofstream out(temp);
    out << content;
  }
  fs::rename(temp, path);
}

std::string hash_content(const std::string& content) {
  std::ostringstream digest;
  std::size_t h = 5381;
  for (unsigned char ch : content) {
    h = ((h << 5) + h) + ch;
  }
  digest << std::hex << h;
  return digest.str();
}

const ZoneSettings* settings_for(const std::vector<ZoneSettings>& settings, int zone) {
  for (const auto& row : settings) {
    if (row.zone_number == zone) {
      return &row;
    }
  }
  return nullptr;
}

std::string render_hook_script(const ServiceConfig& config, int airplay_source) {
  std::ostringstream out;
  out << "#!/usr/bin/env bash\n"
      << "set -euo pipefail\n"
      << "ACTION=\"${1:-}\"\n"
      << "ZONE=\"${2:-}\"\n"
      << "VOLUME_DB=\"${3:-}\"\n"
      << "SOCKET=\"" << config.hifi_socket_path << "\"\n"
      << "PCM_SOCKET=\"" << config.pcm_router_socket_path << "\"\n"
      << "AIRPLAY_SOURCE=\"" << airplay_source << "\"\n"
      << "pcm_route() {\n"
      << "  local method=\"$1\"\n"
      << "  printf '{\"method\":\"%s\",\"zoneId\":%s}\\n' \"${method}\" \"${ZONE}\" | "
      << "nc -U -w 1 \"${PCM_SOCKET}\" | head -1\n"
      << "}\n"
      << "pcm_owner_from_response() {\n"
      << "  python3 -c \"import json,sys; d=json.loads(sys.argv[1]); "
      << "print(d.get('payload',{}).get('ownerZoneId',0))\" \"$1\" 2>/dev/null || echo 0\n"
      << "}\n"
      << "pcm_route_zone() {\n"
      << "  local method=\"$1\"\n"
      << "  local zone_id=\"$2\"\n"
      << "  printf '{\"method\":\"%s\",\"zoneId\":%s}\\n' \"${method}\" \"${zone_id}\" | "
      << "nc -U -w 1 \"${PCM_SOCKET}\" | head -1\n"
      << "}\n"
      << "send_cmd() {\n"
      << "  local cmd=\"$1\"\n"
      << "  timeout 2 bash -c \"printf '{\\\"method\\\":\\\"sendCommand\\\","
      << "\\\"correlationId\\\":\\\"shairport-hook\\\",\\\"command\\\":\\\"%s\\\"}\\\\n' "
      << "\\\"${cmd}\\\" | nc -U -w 2 \\\"${SOCKET}\\\" >/dev/null\" 2>/dev/null || true\n"
      << "}\n"
      << "send_cmd_async() {\n"
      << "  send_cmd \"$1\" &\n"
      << "}\n"
      << "nqptp_play_begin() {\n"
      << "  printf '/nqptp B\\n' | nc -u -w 1 127.0.0.1 9000 >/dev/null 2>&1 || true\n"
      << "}\n"
      << "pcm_handoff_on_zone_end() {\n"
      << "  resp=\"$(pcm_route \"route_end\")\"\n"
      << "  fallback_owner=\"$(pcm_owner_from_response \"${resp}\")\"\n"
      << "  if [[ -n \"${fallback_owner}\" && \"${fallback_owner}\" != \"0\" && "
      << "\"${fallback_owner}\" != \"${ZONE}\" ]]; then\n"
      << "    nqptp_play_begin\n"
      << "    send_cmd_async \"*Z${fallback_owner}POWER1\"\n"
      << "    send_cmd_async \"*Z${fallback_owner}SRC${AIRPLAY_SOURCE}\"\n"
      << "  else\n"
      << "    send_cmd_async \"*Z${ZONE}POWER0\"\n"
      << "  fi\n"
      << "}\n"
      << "case \"${ACTION}\" in\n"
      << "  activate)\n"
      << "    send_cmd_async \"*Z${ZONE}POWER1\"\n"
      << "    send_cmd_async \"*Z${ZONE}SRC${AIRPLAY_SOURCE}\"\n"
      << "    ;;\n"
      << "  play_begin)\n"
      << "    pcm_route \"route_start\" >/dev/null || true\n"
      << "    nqptp_play_begin\n"
      << "    send_cmd_async \"*Z${ZONE}POWER1\"\n"
      << "    send_cmd_async \"*Z${ZONE}SRC${AIRPLAY_SOURCE}\"\n"
      << "    ;;\n"
      << "  play_end)\n"
      << "    pcm_handoff_on_zone_end\n"
      << "    ;;\n"
      << "  deactivate)\n"
      << "    send_cmd_async \"*Z${ZONE}POWER0\"\n"
      << "    ;;\n"
      << "  volume)\n"
      << "    db=\"${VOLUME_DB}\"\n"
      << "    zone_id=\"${ZONE}\"\n"
      << "    if [[ -z \"${db}\" && \"${zone_id}\" =~ ^([0-9]+)-([0-9.]+)$ ]]; then\n"
      << "      zone_id=\"${BASH_REMATCH[1]}\"\n"
      << "      db=\"-${BASH_REMATCH[2]}\"\n"
      << "    fi\n"
      << "    if [[ -n \"${db}\" ]]; then\n"
      << "      pct=$(awk -v db=\"${db}\" 'BEGIN { v=((db+30)/30)*100; if (v<0) v=0; "
      << "if (v>100) v=100; printf \"%.0f\", v }')\n"
      << "      send_cmd \"*Z${zone_id}VOLUME${pct}\"\n"
      << "    fi\n"
      << "    ;;\n"
      << "  error)\n"
      << "    logger -t homepi-shairport-error \"zone=${ZONE} unfixable error\"\n"
      << "    ;;\n"
      << "esac\n";
  return out.str();
}

}  // namespace

ConfigGenerator::ConfigGenerator(ServiceConfig config) : config_(std::move(config)) {}

std::map<int, std::string> ConfigGenerator::generate(const std::vector<ZoneRow>& zones,
                                                     const std::vector<ZoneSettings>& settings,
                                                     int airplay_source,
                                                     const homepi::storage::AudioProfileTuple&
                                                         loopback_profile) const {
  std::map<int, std::string> hashes;
  const std::string hook_script = render_hook_script(config_, airplay_source);
  write_executable(fs::path(config_.hooks_dir) / "homepi-shairport-hook.sh", hook_script);

  for (const auto& zone : zones) {
    if (zone.zone_number < 1 || zone.zone_number > config_.zone_count) {
      continue;
    }
    if (!zone.enabled.has_value() || zone.enabled.value() != 1) {
      continue;
    }
    const ZoneSettings* zone_settings = settings_for(settings, zone.zone_number);
    const double active_timeout = zone_settings ? zone_settings->active_state_timeout : 1.0;
    const int session_timeout = zone_settings ? zone_settings->session_timeout : 60;
    const int log_verbosity = zone_settings ? zone_settings->log_verbosity : 1;
    const std::string profile =
        zone_settings ? zone_settings->volume_control_profile : "standard";
    const std::string zone_name =
        zone.name.has_value() && !zone.name->empty()
            ? *zone.name
            : ("HomePi Zone " + std::to_string(zone.zone_number));
    const int inivol = zone.initial_volume.value_or(50);
    const double default_volume = inivol_to_apple_db(inivol);
    const std::string hook = hook_path(config_, "homepi-shairport-hook.sh");
    const int port = 7000 + zone.zone_number;

    std::ostringstream conf;
    conf << std::fixed << std::setprecision(1);
    conf << "general =\n"
         << "{\n"
         << "  name = \"" << zone_name << "\";\n"
         << "  interpolation = \"SOXR\";\n"
         << "  output_backend = \"alsa\";\n"
         << "  mdns_backend = \"avahi\";\n"
         << "  port = " << port << ";\n"
         << "  airplay_device_id_offset = " << zone.zone_number << ";\n"
         << "  regtype = \"_airplay._tcp\";\n"
         << "  ignore_volume_control = \"yes\";\n"
         << "  volume_control_profile = \"" << profile << "\";\n"
         << "  default_airplay_volume = " << default_volume << ";\n"
         << "  audio_backend_buffer_desired_length_in_seconds = 0.2;\n"
         << "  run_this_when_volume_is_set = \"" << hook << " volume " << zone.zone_number
         << " \";\n"
         << "};\n\n"
         << "sessioncontrol =\n"
         << "{\n"
         << "  run_this_before_entering_active_state = \"" << hook << " activate "
         << zone.zone_number << "\";\n"
         << "  run_this_after_exiting_active_state = \"" << hook << " deactivate "
         << zone.zone_number << "\";\n"
         << "  active_state_timeout = " << active_timeout << ";\n"
         << "  run_this_before_play_begins = \"" << hook << " play_begin " << zone.zone_number
         << "\";\n"
         << "  run_this_after_play_ends = \"" << hook << " play_end " << zone.zone_number
         << "\";\n"
         << "  run_this_if_an_unfixable_error_is_detected = \"" << hook << " error "
         << zone.zone_number << "\";\n"
         << "  wait_for_completion = \"no\";\n"
         << "  allow_session_interruption = \"no\";\n"
         << "  session_timeout = " << session_timeout << ";\n"
         << "};\n\n"
         << "// All zones share the system NQPTP master clock via /dev/shm/nqptp.\n"
         << "// homepi-nqptp.service must be running before shairport zones start.\n\n"
         << "alsa =\n"
         << "{\n"
         << "  output_device = \"" << alsa_output_device(zone.zone_number) << "\";\n"
         << "  output_rate = " << loopback_profile.sample_rate << ";\n"
         << "  output_format = \"" << homepi::storage::sample_format_to_string(loopback_profile.sample_format)
         << "\";\n"
         << "};\n\n"
         << "metadata =\n"
         << "{\n"
         << "  enabled = \"yes\";\n"
         << "  include_cover_art = \"yes\";\n"
         << "  cover_art_cache_directory = \"\";\n"
         << "  pipe_name = \"/tmp/homepi-metadata-zone-" << zone.zone_number << "\";\n"
         << "  progress_interval = 5.0;\n"
         << "};\n\n"
         << "mqtt =\n"
         << "{\n"
         << "  enabled = \"yes\";\n"
         << "  hostname = \"127.0.0.1\";\n"
         << "  port = 1883;\n"
         << "  topic = \"shairport/zone/" << zone.zone_number << "\";\n"
         << "  publish_raw = \"yes\";\n"
         << "  publish_parsed = \"yes\";\n"
         << "  publish_cover = \"yes\";\n"
         << "  publish_retain = \"yes\";\n"
         << "  enable_autodiscovery = \"yes\";\n"
         << "  autodiscovery_prefix = \"homepi\";\n"
         << "  enable_remote = \"yes\";\n"
         << "};\n\n"
         << "diagnostic =\n"
         << "{\n"
         << "  log_output_to = \"stdout\";\n"
         << "  log_verbosity = " << log_verbosity << ";\n"
         << "};\n";

    const std::string body = conf.str();
    const fs::path path =
        fs::path(config_.zones_config_dir) / ("zone-" + std::to_string(zone.zone_number) + ".conf");
    write_file(path, body);
    hashes[zone.zone_number] = hash_content(body + std::to_string(airplay_source));
  }

  return hashes;
}

}  // namespace homepi::shairport_sync
