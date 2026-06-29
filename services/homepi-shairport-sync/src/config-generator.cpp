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

std::string render_hook_script(int airplay_source) {
  std::ostringstream out;
  out << "#!/usr/bin/env bash\n"
      << "set -euo pipefail\n"
      << "export AIRPLAY_SOURCE=\"" << airplay_source << "\"\n"
      << "export HOMEPI_EVENTS_SOCKET=\"/run/homepi/broker/broker.sock\"\n"
      << "exec \"/opt/homepi/services/shairport/bin/homepi-shairport-hook\" \"$@\"\n";
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
  const std::string hook_script = render_hook_script(airplay_source);
  write_executable(fs::path(config_.hooks_dir) / "homepi-shairport-hook.sh", hook_script);

  for (const auto& zone : zones) {
    if (zone.zone_number < 1 || zone.zone_number > config_.zone_count) {
      continue;
    }
    if (!zone.enabled.has_value() || zone.enabled.value() != 1) {
      continue;
    }
    const ZoneSettings* zone_settings = settings_for(settings, zone.zone_number);
    double active_timeout = zone_settings ? zone_settings->active_state_timeout : 0.5;
    if (active_timeout > 1.0) {
      active_timeout = 0.5;
    }
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
         << "  audio_backend_buffer_desired_length_in_seconds = 0.05;\n"
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
         << "  progress_interval = 1.0;\n"
         << "};\n\n"
         << "mqtt =\n"
         << "{\n"
         << "  enabled = \"no\";\n"
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
