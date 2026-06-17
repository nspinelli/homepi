#include "homepi/usb-devices/audio-profile-writer.hpp"

#include <chrono>
#include <ctime>
#include <filesystem>
#include <fstream>
#include <sstream>
#include <sqlite3.h>

#include "homepi/usb-devices/platform-loopback-policy.hpp"

namespace fs = std::filesystem;

namespace homepi::usb_devices {

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

std::string tuple_format_string(homepi::storage::SampleFormat format) {
  return homepi::storage::sample_format_to_string(format);
}

}  // namespace

AudioProfileWriter::AudioProfileWriter(sqlite3* db, std::string generated_dir,
                                       std::string primary_alsa_id)
    : db_(db), generated_dir_(std::move(generated_dir)), primary_alsa_id_(std::move(primary_alsa_id)) {}

void AudioProfileWriter::upsert_capabilities(
    const homepi::storage::AudioCapabilities& capabilities) {
  const char* upsert_sql =
      "INSERT INTO usb_audio_capabilities (device_id, probed_at, probe_error) VALUES (?, ?, ?) "
      "ON CONFLICT(device_id) DO UPDATE SET probed_at = excluded.probed_at, probe_error = "
      "excluded.probe_error";
  sqlite3_stmt* stmt = nullptr;
  sqlite3_prepare_v2(db_, upsert_sql, -1, &stmt, nullptr);
  sqlite3_bind_text(stmt, 1, capabilities.device_id.c_str(), -1, SQLITE_TRANSIENT);
  sqlite3_bind_text(stmt, 2, capabilities.probed_at.c_str(), -1, SQLITE_TRANSIENT);
  if (capabilities.probe_error.has_value()) {
    sqlite3_bind_text(stmt, 3, capabilities.probe_error->c_str(), -1, SQLITE_TRANSIENT);
  } else {
    sqlite3_bind_null(stmt, 3);
  }
  sqlite3_step(stmt);
  sqlite3_finalize(stmt);

  const char* delete_sql = "DELETE FROM supported_profile_tuples WHERE device_id = ?;";
  sqlite3_prepare_v2(db_, delete_sql, -1, &stmt, nullptr);
  sqlite3_bind_text(stmt, 1, capabilities.device_id.c_str(), -1, SQLITE_TRANSIENT);
  sqlite3_step(stmt);
  sqlite3_finalize(stmt);

  const char* tuple_sql =
      "INSERT INTO supported_profile_tuples (device_id, sample_rate, channels, sample_format) "
      "VALUES (?, ?, ?, ?)";
  sqlite3_prepare_v2(db_, tuple_sql, -1, &stmt, nullptr);
  for (const auto& tuple : capabilities.supported_profile_tuples) {
    sqlite3_bind_text(stmt, 1, capabilities.device_id.c_str(), -1, SQLITE_TRANSIENT);
    sqlite3_bind_int(stmt, 2, static_cast<int>(tuple.sample_rate));
    sqlite3_bind_int(stmt, 3, tuple.channels);
    sqlite3_bind_text(stmt, 4, tuple_format_string(tuple.sample_format).c_str(), -1,
                      SQLITE_TRANSIENT);
    sqlite3_step(stmt);
    sqlite3_reset(stmt);
  }
  sqlite3_finalize(stmt);
}

uint64_t AudioProfileWriter::bump_revision(const std::string& status) {
  const std::string now = utc_now();
  sqlite3_stmt* stmt = nullptr;
  sqlite3_prepare_v2(db_, "UPDATE audio_profile_state SET profile_revision = profile_revision + 1, "
                          "profile_status = ?, updated_at = ? WHERE id = 1;",
                     -1, &stmt, nullptr);
  sqlite3_bind_text(stmt, 1, status.c_str(), -1, SQLITE_TRANSIENT);
  sqlite3_bind_text(stmt, 2, now.c_str(), -1, SQLITE_TRANSIENT);
  sqlite3_step(stmt);
  sqlite3_finalize(stmt);
  return current_revision();
}

uint64_t AudioProfileWriter::current_revision() const {
  sqlite3_stmt* stmt = nullptr;
  sqlite3_prepare_v2(db_, "SELECT profile_revision FROM audio_profile_state WHERE id = 1;", -1,
                     &stmt, nullptr);
  uint64_t revision = 0;
  if (sqlite3_step(stmt) == SQLITE_ROW) {
    revision = static_cast<uint64_t>(sqlite3_column_int64(stmt, 0));
  }
  sqlite3_finalize(stmt);
  return revision;
}

std::string AudioProfileWriter::stable_primary_hw() const {
  return "hw:" + primary_alsa_id_ + ",0";
}

uint64_t AudioProfileWriter::apply_platform_policy() {
  PlatformLoopbackPolicy policy;
  const auto tuple = policy.default_loopback_profile();
  const std::string now = utc_now();

  sqlite3_exec(db_, "DELETE FROM audio_operating_profiles WHERE role = 'primary_audio';", nullptr,
               nullptr, nullptr);

  const char* loopback_sql =
      "INSERT INTO audio_operating_profiles (role, device_id, alsa_device, sample_rate, channels, "
      "sample_format, profile_source, updated_at) VALUES ('platform_loopback', NULL, NULL, ?, ?, "
      "?, 'platform_policy', ?) ON CONFLICT(role) DO UPDATE SET sample_rate = excluded.sample_rate, "
      "channels = excluded.channels, sample_format = excluded.sample_format, profile_source = "
      "excluded.profile_source, updated_at = excluded.updated_at";
  sqlite3_stmt* stmt = nullptr;
  sqlite3_prepare_v2(db_, loopback_sql, -1, &stmt, nullptr);
  sqlite3_bind_int(stmt, 1, static_cast<int>(tuple.sample_rate));
  sqlite3_bind_int(stmt, 2, tuple.channels);
  sqlite3_bind_text(stmt, 3, tuple_format_string(tuple.sample_format).c_str(), -1, SQLITE_TRANSIENT);
  sqlite3_bind_text(stmt, 4, now.c_str(), -1, SQLITE_TRANSIENT);
  sqlite3_step(stmt);
  sqlite3_finalize(stmt);

  homepi::storage::ActiveAudioConfig config;
  config.mode = homepi::storage::ProfileMode::NoDacAssigned;
  config.status = homepi::storage::ProfileStatus::Active;
  config.loopback_profile = tuple;
  config.profile_source = homepi::storage::ProfileSource::PlatformPolicy;
  const uint64_t revision = bump_revision("active");
  config.profile_revision = revision;
  write_artifact(config);
  return revision;
}

uint64_t AudioProfileWriter::apply_primary_profile(
    const UsbDevice& device, const homepi::storage::AudioProfileTuple& tuple) {
  const std::string now = utc_now();
  const std::string alsa_device = stable_primary_hw();

  auto upsert_role = [&](const char* role, const char* source) {
    const char* sql =
        "INSERT INTO audio_operating_profiles (role, device_id, alsa_device, sample_rate, channels, "
        "sample_format, profile_source, updated_at) VALUES (?, ?, ?, ?, ?, ?, ?, ?) "
        "ON CONFLICT(role) DO UPDATE SET device_id = excluded.device_id, alsa_device = "
        "excluded.alsa_device, sample_rate = excluded.sample_rate, channels = excluded.channels, "
        "sample_format = excluded.sample_format, profile_source = excluded.profile_source, "
        "updated_at = excluded.updated_at";
    sqlite3_stmt* stmt = nullptr;
    sqlite3_prepare_v2(db_, sql, -1, &stmt, nullptr);
    sqlite3_bind_text(stmt, 1, role, -1, SQLITE_TRANSIENT);
    sqlite3_bind_text(stmt, 2, device.device_id.c_str(), -1, SQLITE_TRANSIENT);
    sqlite3_bind_text(stmt, 3, alsa_device.c_str(), -1, SQLITE_TRANSIENT);
    sqlite3_bind_int(stmt, 4, static_cast<int>(tuple.sample_rate));
    sqlite3_bind_int(stmt, 5, tuple.channels);
    sqlite3_bind_text(stmt, 6, tuple_format_string(tuple.sample_format).c_str(), -1,
                      SQLITE_TRANSIENT);
    sqlite3_bind_text(stmt, 7, source, -1, SQLITE_TRANSIENT);
    sqlite3_bind_text(stmt, 8, now.c_str(), -1, SQLITE_TRANSIENT);
    sqlite3_step(stmt);
    sqlite3_finalize(stmt);
  };

  upsert_role("primary_audio", "user_selected");
  upsert_role("platform_loopback", "user_selected");

  homepi::storage::ActiveAudioConfig config;
  config.mode = homepi::storage::ProfileMode::DacAssigned;
  config.status = homepi::storage::ProfileStatus::Active;
  config.loopback_profile = tuple;
  config.dac_profile = tuple;
  config.alsa_dac_device = alsa_device;
  config.primary_device_id = device.device_id;
  config.profile_source = homepi::storage::ProfileSource::UserSelected;
  const uint64_t revision = bump_revision("active");
  config.profile_revision = revision;
  write_artifact(config);
  return revision;
}

uint64_t AudioProfileWriter::mark_profile_paused_invalid() {
  homepi::storage::ActiveAudioConfig config;
  config.mode = homepi::storage::ProfileMode::NoDacAssigned;
  config.loopback_profile = PlatformLoopbackPolicy().default_loopback_profile();
  config.profile_source = homepi::storage::ProfileSource::PlatformPolicy;

  sqlite3_stmt* stmt = nullptr;
  const char* primary_sql =
      "SELECT device_id, alsa_device, sample_rate, channels, sample_format "
      "FROM audio_operating_profiles WHERE role = 'primary_audio'";
  if (sqlite3_prepare_v2(db_, primary_sql, -1, &stmt, nullptr) == SQLITE_OK &&
      sqlite3_step(stmt) == SQLITE_ROW) {
    if (const char* device_id = reinterpret_cast<const char*>(sqlite3_column_text(stmt, 0))) {
      config.primary_device_id = device_id;
    }
    if (const char* alsa = reinterpret_cast<const char*>(sqlite3_column_text(stmt, 1))) {
      config.alsa_dac_device = alsa;
    }
    homepi::storage::AudioProfileTuple tuple;
    tuple.sample_rate = static_cast<uint32_t>(sqlite3_column_int(stmt, 2));
    tuple.channels = static_cast<uint16_t>(sqlite3_column_int(stmt, 3));
    const char* format = reinterpret_cast<const char*>(sqlite3_column_text(stmt, 4));
    tuple.sample_format =
        homepi::storage::parse_sample_format(format != nullptr ? format : "")
            .value_or(homepi::storage::SampleFormat::S16Le);
    config.dac_profile = tuple;
    config.loopback_profile = tuple;
    config.mode = homepi::storage::ProfileMode::DacAssigned;
    config.profile_source = homepi::storage::ProfileSource::UserSelected;
  }
  sqlite3_finalize(stmt);

  const uint64_t revision = bump_revision("paused_invalid");
  config.profile_revision = revision;
  config.status = homepi::storage::ProfileStatus::PausedInvalid;
  write_artifact(config);
  return revision;
}

void AudioProfileWriter::write_artifact(const homepi::storage::ActiveAudioConfig& config) {
  const fs::path artifact_dir = fs::path(generated_dir_) / "audio";
  std::error_code ec;
  fs::create_directories(artifact_dir, ec);
  const fs::path temp_path = artifact_dir / "operating-profile.json.tmp";
  const fs::path final_path = artifact_dir / "operating-profile.json";

  std::ostringstream json;
  json << "{";
  json << "\"mode\":\"" << (config.mode == homepi::storage::ProfileMode::DacAssigned
                                ? "dac_assigned"
                                : "no_dac_assigned")
       << "\",";
  json << "\"profileRevision\":" << config.profile_revision << ",";
  json << "\"profileSource\":\""
       << homepi::storage::profile_source_to_string(config.profile_source) << "\",";
  json << "\"profileStatus\":\""
       << (config.status == homepi::storage::ProfileStatus::PausedInvalid ? "paused_invalid"
                                                                          : "active")
       << "\",";
  json << "\"loopbackProfile\":{"
       << "\"sampleRate\":" << config.loopback_profile.sample_rate << ","
       << "\"channels\":" << config.loopback_profile.channels << ","
       << "\"sampleFormat\":\""
       << tuple_format_string(config.loopback_profile.sample_format) << "\"},";
  if (config.dac_profile.has_value()) {
    json << "\"primaryAudio\":{"
         << "\"deviceId\":\"" << config.primary_device_id << "\","
         << "\"alsaDevice\":\"" << config.alsa_dac_device << "\","
         << "\"sampleRate\":" << config.dac_profile->sample_rate << ","
         << "\"channels\":" << config.dac_profile->channels << ","
         << "\"sampleFormat\":\""
         << tuple_format_string(config.dac_profile->sample_format) << "\","
         << "\"source\":\"user_selected\"},";
  } else {
    json << "\"primaryAudio\":null,";
  }
  json << "\"updatedAt\":\"" << utc_now() << "\"}";

  {
    std::ofstream out(temp_path);
    out << json.str();
  }
  fs::rename(temp_path, final_path, ec);
}

}  // namespace homepi::usb_devices
