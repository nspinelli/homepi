#include "homepi/storage/audio-profile-repository.hpp"

#include <cstdlib>
#include <fstream>
#include <sstream>
#include <sqlite3.h>

#include "homepi/storage/database-connection.hpp"
#include "homepi/storage/repository-error.hpp"

namespace homepi::storage {

namespace {

AudioProfileTuple tuple_from_columns(int rate, int channels, const char* format) {
  AudioProfileTuple tuple;
  tuple.sample_rate = static_cast<uint32_t>(rate);
  tuple.channels = static_cast<uint16_t>(channels);
  const std::optional<SampleFormat> parsed =
      format != nullptr ? parse_sample_format(format) : std::nullopt;
  tuple.sample_format = parsed.value_or(SampleFormat::S16Le);
  return tuple;
}

std::string extract_json_string(const std::string& json, const std::string& key) {
  const std::string needle = "\"" + key + "\":\"";
  const size_t start = json.find(needle);
  if (start == std::string::npos) {
    return "";
  }
  const size_t value_start = start + needle.size();
  const size_t value_end = json.find('"', value_start);
  if (value_end == std::string::npos) {
    return "";
  }
  return json.substr(value_start, value_end - value_start);
}

int extract_json_int(const std::string& json, const std::string& key) {
  const std::string needle = "\"" + key + "\":";
  const size_t start = json.find(needle);
  if (start == std::string::npos) {
    return 0;
  }
  return std::atoi(json.c_str() + start + needle.size());
}

AudioProfileTuple tuple_from_json_object(const std::string& json) {
  AudioProfileTuple tuple;
  tuple.sample_rate = static_cast<uint32_t>(extract_json_int(json, "sampleRate"));
  tuple.channels = static_cast<uint16_t>(extract_json_int(json, "channels"));
  const std::string format = extract_json_string(json, "sampleFormat");
  tuple.sample_format = parse_sample_format(format).value_or(SampleFormat::S16Le);
  return tuple;
}

}  // namespace

AudioProfileRepository::AudioProfileRepository(DatabaseConnection& db) : db_(db) {}

std::optional<AudioCapabilities> AudioProfileRepository::get_capabilities(
    const std::string& device_id) const {
  const char* sql =
      "SELECT probed_at, probe_error FROM usb_audio_capabilities WHERE device_id = ?;";
  sqlite3_stmt* stmt = nullptr;
  if (sqlite3_prepare_v2(db_.handle(), sql, -1, &stmt, nullptr) != SQLITE_OK) {
    throw RepositoryError(sqlite3_errmsg(db_.handle()));
  }
  sqlite3_bind_text(stmt, 1, device_id.c_str(), -1, SQLITE_TRANSIENT);
  if (sqlite3_step(stmt) != SQLITE_ROW) {
    sqlite3_finalize(stmt);
    return std::nullopt;
  }

  AudioCapabilities caps;
  caps.device_id = device_id;
  const char* probed_at = reinterpret_cast<const char*>(sqlite3_column_text(stmt, 0));
  caps.probed_at = probed_at != nullptr ? probed_at : "";
  const char* probe_error = reinterpret_cast<const char*>(sqlite3_column_text(stmt, 1));
  if (probe_error != nullptr && probe_error[0] != '\0') {
    caps.probe_error = probe_error;
  }
  sqlite3_finalize(stmt);

  const char* tuple_sql =
      "SELECT sample_rate, channels, sample_format FROM supported_profile_tuples "
      "WHERE device_id = ? ORDER BY sample_rate, channels, sample_format;";
  if (sqlite3_prepare_v2(db_.handle(), tuple_sql, -1, &stmt, nullptr) != SQLITE_OK) {
    throw RepositoryError(sqlite3_errmsg(db_.handle()));
  }
  sqlite3_bind_text(stmt, 1, device_id.c_str(), -1, SQLITE_TRANSIENT);
  while (sqlite3_step(stmt) == SQLITE_ROW) {
    caps.supported_profile_tuples.push_back(tuple_from_columns(
        sqlite3_column_int(stmt, 0), sqlite3_column_int(stmt, 1),
        reinterpret_cast<const char*>(sqlite3_column_text(stmt, 2))));
  }
  sqlite3_finalize(stmt);
  return caps;
}

ActiveAudioConfig AudioProfileRepository::load_active_config() const {
  ActiveAudioConfig config;
  config.loopback_profile = platform_loopback_default();
  config.profile_source = ProfileSource::PlatformPolicy;
  config.mode = ProfileMode::NoDacAssigned;

  const char* meta_sql =
      "SELECT profile_revision, profile_status FROM audio_profile_state WHERE id = 1;";
  sqlite3_stmt* stmt = nullptr;
  if (sqlite3_prepare_v2(db_.handle(), meta_sql, -1, &stmt, nullptr) == SQLITE_OK &&
      sqlite3_step(stmt) == SQLITE_ROW) {
    config.profile_revision = static_cast<uint64_t>(sqlite3_column_int64(stmt, 0));
    const char* status = reinterpret_cast<const char*>(sqlite3_column_text(stmt, 1));
    if (status != nullptr && std::string(status) == "paused_invalid") {
      config.status = ProfileStatus::PausedInvalid;
    }
  }
  if (stmt != nullptr) {
    sqlite3_finalize(stmt);
    stmt = nullptr;
  }

  const char* loopback_sql =
      "SELECT sample_rate, channels, sample_format, profile_source FROM audio_operating_profiles "
      "WHERE role = 'platform_loopback';";
  if (sqlite3_prepare_v2(db_.handle(), loopback_sql, -1, &stmt, nullptr) == SQLITE_OK &&
      sqlite3_step(stmt) == SQLITE_ROW) {
    config.loopback_profile = tuple_from_columns(
        sqlite3_column_int(stmt, 0), sqlite3_column_int(stmt, 1),
        reinterpret_cast<const char*>(sqlite3_column_text(stmt, 2)));
    const char* source = reinterpret_cast<const char*>(sqlite3_column_text(stmt, 3));
    if (source != nullptr) {
      config.profile_source = parse_profile_source(source).value_or(ProfileSource::PlatformPolicy);
    }
  }
  if (stmt != nullptr) {
    sqlite3_finalize(stmt);
    stmt = nullptr;
  }

  const char* primary_sql =
      "SELECT device_id, alsa_device, sample_rate, channels, sample_format, profile_source "
      "FROM audio_operating_profiles WHERE role = 'primary_audio';";
  if (sqlite3_prepare_v2(db_.handle(), primary_sql, -1, &stmt, nullptr) == SQLITE_OK &&
      sqlite3_step(stmt) == SQLITE_ROW) {
    const char* device_id = reinterpret_cast<const char*>(sqlite3_column_text(stmt, 0));
    const char* alsa_device = reinterpret_cast<const char*>(sqlite3_column_text(stmt, 1));
    if (device_id != nullptr) {
      config.primary_device_id = device_id;
    }
    if (alsa_device != nullptr) {
      config.alsa_dac_device = alsa_device;
    }
    const AudioProfileTuple dac_tuple = tuple_from_columns(
        sqlite3_column_int(stmt, 2), sqlite3_column_int(stmt, 3),
        reinterpret_cast<const char*>(sqlite3_column_text(stmt, 4)));
    config.dac_profile = dac_tuple;
    config.loopback_profile = dac_tuple;
    config.mode = ProfileMode::DacAssigned;
    const char* source = reinterpret_cast<const char*>(sqlite3_column_text(stmt, 5));
    if (source != nullptr) {
      config.profile_source = parse_profile_source(source).value_or(ProfileSource::UserSelected);
    }
  }
  if (stmt != nullptr) {
    sqlite3_finalize(stmt);
  }

  return config;
}

std::optional<ActiveAudioConfig> AudioProfileRepository::load_from_artifact(
    const std::string& artifact_path) {
  std::ifstream input(artifact_path);
  if (!input.is_open()) {
    return std::nullopt;
  }
  std::ostringstream buffer;
  buffer << input.rdbuf();
  const std::string json = buffer.str();
  if (json.empty()) {
    return std::nullopt;
  }

  ActiveAudioConfig config;
  config.profile_revision = static_cast<uint64_t>(extract_json_int(json, "profileRevision"));
  const std::string mode = extract_json_string(json, "mode");
  config.mode = mode == "dac_assigned" ? ProfileMode::DacAssigned : ProfileMode::NoDacAssigned;
  const std::string status = extract_json_string(json, "profileStatus");
  config.status = status == "paused_invalid" ? ProfileStatus::PausedInvalid : ProfileStatus::Active;
  const std::string source = extract_json_string(json, "profileSource");
  config.profile_source =
      parse_profile_source(source).value_or(ProfileSource::PlatformPolicy);

  const std::string loopback_key = "\"loopbackProfile\":";
  const size_t loopback_pos = json.find(loopback_key);
  if (loopback_pos != std::string::npos) {
    const size_t object_start = json.find('{', loopback_pos);
    const size_t object_end = json.find('}', object_start);
    if (object_start != std::string::npos && object_end != std::string::npos) {
      config.loopback_profile = tuple_from_json_object(json.substr(object_start, object_end - object_start + 1));
    }
  }

  const std::string primary_key = "\"primaryAudio\":";
  const size_t primary_pos = json.find(primary_key);
  if (primary_pos != std::string::npos) {
    const size_t null_pos = json.find("null", primary_pos);
    const size_t object_start = json.find('{', primary_pos);
    if (object_start != std::string::npos && (null_pos == std::string::npos || object_start < null_pos)) {
      const size_t object_end = json.find('}', object_start);
      if (object_end != std::string::npos) {
        const std::string primary_object = json.substr(object_start, object_end - object_start + 1);
        config.primary_device_id = extract_json_string(primary_object, "deviceId");
        config.alsa_dac_device = extract_json_string(primary_object, "alsaDevice");
        config.dac_profile = tuple_from_json_object(primary_object);
        config.loopback_profile = *config.dac_profile;
        config.mode = ProfileMode::DacAssigned;
        config.profile_source = ProfileSource::UserSelected;
      }
    }
  }

  return config;
}

}  // namespace homepi::storage
