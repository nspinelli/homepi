#include "homepi/audio-paging/paging-repository.hpp"

#include <chrono>
#include <ctime>
#include <sqlite3.h>

#include <sstream>
#include <stdexcept>

#include "homepi/audio-paging/json-utils.hpp"

namespace homepi::audio_paging {

namespace {

bool to_bool(int value) { return value != 0; }

PagingIdlePolicy idle_policy_from_sqlite(const char* value) {
  if (value == nullptr) {
    return PagingIdlePolicy::AlwaysWarm;
  }
  return parse_idle_policy(value, PagingIdlePolicy::AlwaysWarm);
}

}  // namespace

PagingRepository::PagingRepository(std::string database_path, std::string migration_sql)
    : database_path_(std::move(database_path)) {
  if (sqlite3_open(database_path_.c_str(), &db_) != SQLITE_OK) {
    throw std::runtime_error("Failed to open sqlite db: " + database_path_);
  }
  sqlite3_busy_timeout(db_, 10000);
  char* error = nullptr;
  if (sqlite3_exec(db_, migration_sql.c_str(), nullptr, nullptr, &error) != SQLITE_OK) {
    const std::string message = error == nullptr ? "migration failed" : error;
    sqlite3_free(error);
    throw std::runtime_error(message);
  }
}

PagingRepository::~PagingRepository() {
  std::lock_guard<std::mutex> lock(mutex_);
  if (db_ != nullptr) {
    sqlite3_close(db_);
    db_ = nullptr;
  }
}

std::string PagingRepository::now_utc() {
  const auto now = std::chrono::system_clock::now();
  const std::time_t time = std::chrono::system_clock::to_time_t(now);
  std::tm tm{};
  gmtime_r(&time, &tm);
  char buffer[32];
  std::strftime(buffer, sizeof(buffer), "%Y-%m-%dT%H:%M:%SZ", &tm);
  return std::string(buffer);
}

bool PagingRepository::exec_locked(const std::string& sql) const {
  char* error = nullptr;
  const int rc = sqlite3_exec(db_, sql.c_str(), nullptr, nullptr, &error);
  if (error != nullptr) {
    sqlite3_free(error);
  }
  return rc == SQLITE_OK;
}

PagingConfig PagingRepository::get_config() const {
  std::lock_guard<std::mutex> lock(mutex_);
  PagingConfig config;
  sqlite3_stmt* stmt = nullptr;
  const char* sql =
      "SELECT ENABLED, DEFAULT_VOICE_ID, ACTIVE_VOICE_ID, ACTIVE_CHIME_ID, MAX_INSTALLED_VOICES, "
      "MAX_TEXT_LENGTH, MAX_PREVIEW_TEXT_LENGTH, STREAM_THRESHOLD_CHARS, MIN_PREROLL_MS, "
      "DEFAULT_ON_BUSY, IDLE_POLICY, IDLE_WARM_TIMEOUT_MS, DAC_IDLE_CLOSE_DELAY_MS, "
      "KEEP_LAST_AUDIO_FOR_DEBUG, CREATED_AT, UPDATED_AT FROM AUDIO_PAGING_CONFIG WHERE ID = 1";
  if (sqlite3_prepare_v2(db_, sql, -1, &stmt, nullptr) != SQLITE_OK) {
    return config;
  }
  if (sqlite3_step(stmt) == SQLITE_ROW) {
    config.enabled = to_bool(sqlite3_column_int(stmt, 0));
    if (const char* value = reinterpret_cast<const char*>(sqlite3_column_text(stmt, 1))) {
      config.default_voice_id = value;
    }
    if (const char* value = reinterpret_cast<const char*>(sqlite3_column_text(stmt, 2))) {
      config.active_voice_id = value;
    }
    if (const char* value = reinterpret_cast<const char*>(sqlite3_column_text(stmt, 3))) {
      config.active_chime_id = value;
    }
    config.max_installed_voices = sqlite3_column_int(stmt, 4);
    config.max_text_length = sqlite3_column_int(stmt, 5);
    config.max_preview_text_length = sqlite3_column_int(stmt, 6);
    config.stream_threshold_chars = sqlite3_column_int(stmt, 7);
    config.min_preroll_ms = sqlite3_column_int(stmt, 8);
    if (const char* value = reinterpret_cast<const char*>(sqlite3_column_text(stmt, 9))) {
      config.default_on_busy = value;
    }
    config.idle_policy = idle_policy_from_sqlite(
        reinterpret_cast<const char*>(sqlite3_column_text(stmt, 10)));
    config.idle_warm_timeout_ms = sqlite3_column_int(stmt, 11);
    config.dac_idle_close_delay_ms = sqlite3_column_int(stmt, 12);
    config.keep_last_audio_for_debug = to_bool(sqlite3_column_int(stmt, 13));
    if (const char* value = reinterpret_cast<const char*>(sqlite3_column_text(stmt, 14))) {
      config.created_at = value;
    }
    if (const char* value = reinterpret_cast<const char*>(sqlite3_column_text(stmt, 15))) {
      config.updated_at = value;
    }
  }
  sqlite3_finalize(stmt);
  return config;
}

bool PagingRepository::update_config(const PagingConfig& config) {
  std::lock_guard<std::mutex> lock(mutex_);
  sqlite3_stmt* stmt = nullptr;
  const char* sql =
      "UPDATE AUDIO_PAGING_CONFIG SET ENABLED = ?, DEFAULT_VOICE_ID = ?, ACTIVE_VOICE_ID = ?, "
      "ACTIVE_CHIME_ID = ?, IDLE_POLICY = ?, IDLE_WARM_TIMEOUT_MS = ?, DAC_IDLE_CLOSE_DELAY_MS = ?, "
      "UPDATED_AT = ? WHERE ID = 1";
  if (sqlite3_prepare_v2(db_, sql, -1, &stmt, nullptr) != SQLITE_OK) {
    return false;
  }
  sqlite3_bind_int(stmt, 1, config.enabled ? 1 : 0);
  sqlite3_bind_text(stmt, 2, config.default_voice_id.c_str(), -1, SQLITE_TRANSIENT);
  sqlite3_bind_text(stmt, 3, config.active_voice_id.c_str(), -1, SQLITE_TRANSIENT);
  sqlite3_bind_text(stmt, 4, config.active_chime_id.c_str(), -1, SQLITE_TRANSIENT);
  const std::string idle_policy = idle_policy_to_string(config.idle_policy);
  sqlite3_bind_text(stmt, 5, idle_policy.c_str(), -1, SQLITE_TRANSIENT);
  sqlite3_bind_int(stmt, 6, config.idle_warm_timeout_ms);
  sqlite3_bind_int(stmt, 7, config.dac_idle_close_delay_ms);
  const std::string now = now_utc();
  sqlite3_bind_text(stmt, 8, now.c_str(), -1, SQLITE_TRANSIENT);
  const bool ok = sqlite3_step(stmt) == SQLITE_DONE;
  sqlite3_finalize(stmt);
  return ok;
}

std::vector<PagingVoice> PagingRepository::list_voices() const {
  std::lock_guard<std::mutex> lock(mutex_);
  std::vector<PagingVoice> voices;
  sqlite3_stmt* stmt = nullptr;
  const char* sql =
      "SELECT VOICE_ID, DISPLAY_NAME, LANGUAGE_CODE, QUALITY, MODEL_PATH, CONFIG_PATH, INSTALLED, "
      "IS_DEFAULT, IS_BUNDLED, LAST_USED_AT, CREATED_AT, UPDATED_AT "
      "FROM AUDIO_PAGING_VOICES ORDER BY IS_DEFAULT DESC, DISPLAY_NAME";
  if (sqlite3_prepare_v2(db_, sql, -1, &stmt, nullptr) != SQLITE_OK) {
    return voices;
  }
  while (sqlite3_step(stmt) == SQLITE_ROW) {
    PagingVoice voice;
    voice.voice_id = reinterpret_cast<const char*>(sqlite3_column_text(stmt, 0));
    voice.display_name = reinterpret_cast<const char*>(sqlite3_column_text(stmt, 1));
    voice.language_code = reinterpret_cast<const char*>(sqlite3_column_text(stmt, 2));
    if (const char* value = reinterpret_cast<const char*>(sqlite3_column_text(stmt, 3))) {
      voice.quality = value;
    }
    voice.model_path = reinterpret_cast<const char*>(sqlite3_column_text(stmt, 4));
    voice.config_path = reinterpret_cast<const char*>(sqlite3_column_text(stmt, 5));
    voice.installed = to_bool(sqlite3_column_int(stmt, 6));
    voice.is_default = to_bool(sqlite3_column_int(stmt, 7));
    voice.is_bundled = to_bool(sqlite3_column_int(stmt, 8));
    if (const char* value = reinterpret_cast<const char*>(sqlite3_column_text(stmt, 9))) {
      voice.last_used_at = value;
    }
    if (const char* value = reinterpret_cast<const char*>(sqlite3_column_text(stmt, 10))) {
      voice.created_at = value;
    }
    if (const char* value = reinterpret_cast<const char*>(sqlite3_column_text(stmt, 11))) {
      voice.updated_at = value;
    }
    voices.push_back(std::move(voice));
  }
  sqlite3_finalize(stmt);
  return voices;
}

std::vector<PagingChime> PagingRepository::list_chimes() const {
  std::lock_guard<std::mutex> lock(mutex_);
  std::vector<PagingChime> chimes;
  sqlite3_stmt* stmt = nullptr;
  const char* sql =
      "SELECT CHIME_ID, DISPLAY_NAME, FILE_PATH, DURATION_MS, IS_DEFAULT, IS_BUNDLED, CREATED_AT, "
      "UPDATED_AT FROM AUDIO_PAGING_CHIMES ORDER BY IS_DEFAULT DESC, DISPLAY_NAME";
  if (sqlite3_prepare_v2(db_, sql, -1, &stmt, nullptr) != SQLITE_OK) {
    return chimes;
  }
  while (sqlite3_step(stmt) == SQLITE_ROW) {
    PagingChime chime;
    chime.chime_id = reinterpret_cast<const char*>(sqlite3_column_text(stmt, 0));
    chime.display_name = reinterpret_cast<const char*>(sqlite3_column_text(stmt, 1));
    chime.file_path = reinterpret_cast<const char*>(sqlite3_column_text(stmt, 2));
    chime.duration_ms = sqlite3_column_int(stmt, 3);
    chime.is_default = to_bool(sqlite3_column_int(stmt, 4));
    chime.is_bundled = to_bool(sqlite3_column_int(stmt, 5));
    if (const char* value = reinterpret_cast<const char*>(sqlite3_column_text(stmt, 6))) {
      chime.created_at = value;
    }
    if (const char* value = reinterpret_cast<const char*>(sqlite3_column_text(stmt, 7))) {
      chime.updated_at = value;
    }
    chimes.push_back(std::move(chime));
  }
  sqlite3_finalize(stmt);
  return chimes;
}

std::optional<PagingVoice> PagingRepository::get_voice(const std::string& voice_id) const {
  for (const PagingVoice& voice : list_voices()) {
    if (voice.voice_id == voice_id) {
      return voice;
    }
  }
  return std::nullopt;
}

std::optional<PagingChime> PagingRepository::get_chime(const std::string& chime_id) const {
  for (const PagingChime& chime : list_chimes()) {
    if (chime.chime_id == chime_id) {
      return chime;
    }
  }
  return std::nullopt;
}

bool PagingRepository::set_active_voice(const std::string& voice_id) {
  std::lock_guard<std::mutex> lock(mutex_);
  const std::string now = now_utc();
  sqlite3_stmt* clear_stmt = nullptr;
  sqlite3_prepare_v2(db_, "UPDATE AUDIO_PAGING_VOICES SET IS_DEFAULT = 0", -1, &clear_stmt, nullptr);
  sqlite3_step(clear_stmt);
  sqlite3_finalize(clear_stmt);

  sqlite3_stmt* voice_stmt = nullptr;
  sqlite3_prepare_v2(db_,
                     "UPDATE AUDIO_PAGING_VOICES SET IS_DEFAULT = 1, LAST_USED_AT = ?, UPDATED_AT = ? "
                     "WHERE VOICE_ID = ?",
                     -1, &voice_stmt, nullptr);
  sqlite3_bind_text(voice_stmt, 1, now.c_str(), -1, SQLITE_TRANSIENT);
  sqlite3_bind_text(voice_stmt, 2, now.c_str(), -1, SQLITE_TRANSIENT);
  sqlite3_bind_text(voice_stmt, 3, voice_id.c_str(), -1, SQLITE_TRANSIENT);
  const bool voice_ok = sqlite3_step(voice_stmt) == SQLITE_DONE;
  sqlite3_finalize(voice_stmt);

  sqlite3_stmt* config_stmt = nullptr;
  sqlite3_prepare_v2(db_, "UPDATE AUDIO_PAGING_CONFIG SET DEFAULT_VOICE_ID = ?, ACTIVE_VOICE_ID = ?, "
                          "UPDATED_AT = ? WHERE ID = 1",
                     -1, &config_stmt, nullptr);
  sqlite3_bind_text(config_stmt, 1, voice_id.c_str(), -1, SQLITE_TRANSIENT);
  sqlite3_bind_text(config_stmt, 2, voice_id.c_str(), -1, SQLITE_TRANSIENT);
  sqlite3_bind_text(config_stmt, 3, now.c_str(), -1, SQLITE_TRANSIENT);
  const bool config_ok = sqlite3_step(config_stmt) == SQLITE_DONE;
  sqlite3_finalize(config_stmt);
  return voice_ok && config_ok;
}

bool PagingRepository::set_active_chime(const std::string& chime_id) {
  std::lock_guard<std::mutex> lock(mutex_);
  const std::string now = now_utc();
  sqlite3_stmt* clear_stmt = nullptr;
  sqlite3_prepare_v2(db_, "UPDATE AUDIO_PAGING_CHIMES SET IS_DEFAULT = 0", -1, &clear_stmt, nullptr);
  sqlite3_step(clear_stmt);
  sqlite3_finalize(clear_stmt);

  sqlite3_stmt* chime_stmt = nullptr;
  sqlite3_prepare_v2(db_,
                     "UPDATE AUDIO_PAGING_CHIMES SET IS_DEFAULT = 1, UPDATED_AT = ? "
                     "WHERE CHIME_ID = ?",
                     -1, &chime_stmt, nullptr);
  sqlite3_bind_text(chime_stmt, 1, now.c_str(), -1, SQLITE_TRANSIENT);
  sqlite3_bind_text(chime_stmt, 2, chime_id.c_str(), -1, SQLITE_TRANSIENT);
  const bool chime_ok = sqlite3_step(chime_stmt) == SQLITE_DONE;
  sqlite3_finalize(chime_stmt);

  sqlite3_stmt* config_stmt = nullptr;
  sqlite3_prepare_v2(db_, "UPDATE AUDIO_PAGING_CONFIG SET ACTIVE_CHIME_ID = ?, UPDATED_AT = ? "
                          "WHERE ID = 1",
                     -1, &config_stmt, nullptr);
  sqlite3_bind_text(config_stmt, 1, chime_id.c_str(), -1, SQLITE_TRANSIENT);
  sqlite3_bind_text(config_stmt, 2, now.c_str(), -1, SQLITE_TRANSIENT);
  const bool config_ok = sqlite3_step(config_stmt) == SQLITE_DONE;
  sqlite3_finalize(config_stmt);
  return chime_ok && config_ok;
}

bool PagingRepository::upsert_voice(const PagingVoice& voice) {
  std::lock_guard<std::mutex> lock(mutex_);
  const std::string now = now_utc();
  sqlite3_stmt* stmt = nullptr;
  const char* sql =
      "INSERT INTO AUDIO_PAGING_VOICES "
      "(VOICE_ID, DISPLAY_NAME, LANGUAGE_CODE, QUALITY, MODEL_PATH, CONFIG_PATH, INSTALLED, "
      "IS_DEFAULT, IS_BUNDLED, LAST_USED_AT, CREATED_AT, UPDATED_AT) "
      "VALUES (?, ?, ?, ?, ?, ?, ?, ?, ?, ?, ?, ?) "
      "ON CONFLICT(VOICE_ID) DO UPDATE SET DISPLAY_NAME = excluded.DISPLAY_NAME, "
      "LANGUAGE_CODE = excluded.LANGUAGE_CODE, QUALITY = excluded.QUALITY, "
      "MODEL_PATH = excluded.MODEL_PATH, CONFIG_PATH = excluded.CONFIG_PATH, "
      "INSTALLED = excluded.INSTALLED, IS_DEFAULT = excluded.IS_DEFAULT, "
      "IS_BUNDLED = excluded.IS_BUNDLED, LAST_USED_AT = excluded.LAST_USED_AT, "
      "UPDATED_AT = excluded.UPDATED_AT";
  if (sqlite3_prepare_v2(db_, sql, -1, &stmt, nullptr) != SQLITE_OK) {
    return false;
  }
  sqlite3_bind_text(stmt, 1, voice.voice_id.c_str(), -1, SQLITE_TRANSIENT);
  sqlite3_bind_text(stmt, 2, voice.display_name.c_str(), -1, SQLITE_TRANSIENT);
  sqlite3_bind_text(stmt, 3, voice.language_code.c_str(), -1, SQLITE_TRANSIENT);
  sqlite3_bind_text(stmt, 4, voice.quality.c_str(), -1, SQLITE_TRANSIENT);
  sqlite3_bind_text(stmt, 5, voice.model_path.c_str(), -1, SQLITE_TRANSIENT);
  sqlite3_bind_text(stmt, 6, voice.config_path.c_str(), -1, SQLITE_TRANSIENT);
  sqlite3_bind_int(stmt, 7, voice.installed ? 1 : 0);
  sqlite3_bind_int(stmt, 8, voice.is_default ? 1 : 0);
  sqlite3_bind_int(stmt, 9, voice.is_bundled ? 1 : 0);
  if (voice.last_used_at.empty()) {
    sqlite3_bind_null(stmt, 10);
  } else {
    sqlite3_bind_text(stmt, 10, voice.last_used_at.c_str(), -1, SQLITE_TRANSIENT);
  }
  sqlite3_bind_text(stmt, 11, now.c_str(), -1, SQLITE_TRANSIENT);
  sqlite3_bind_text(stmt, 12, now.c_str(), -1, SQLITE_TRANSIENT);
  const bool ok = sqlite3_step(stmt) == SQLITE_DONE;
  sqlite3_finalize(stmt);
  return ok;
}

bool PagingRepository::remove_voice(const std::string& voice_id) {
  std::lock_guard<std::mutex> lock(mutex_);
  sqlite3_stmt* stmt = nullptr;
  sqlite3_prepare_v2(
      db_, "DELETE FROM AUDIO_PAGING_VOICES WHERE VOICE_ID = ? AND IFNULL(IS_BUNDLED, 0) = 0", -1,
      &stmt, nullptr);
  sqlite3_bind_text(stmt, 1, voice_id.c_str(), -1, SQLITE_TRANSIENT);
  const bool ok = sqlite3_step(stmt) == SQLITE_DONE;
  sqlite3_finalize(stmt);
  return ok;
}

bool PagingRepository::upsert_chime(const PagingChime& chime) {
  std::lock_guard<std::mutex> lock(mutex_);
  const std::string now = now_utc();
  sqlite3_stmt* stmt = nullptr;
  const char* sql =
      "INSERT INTO AUDIO_PAGING_CHIMES "
      "(CHIME_ID, DISPLAY_NAME, FILE_PATH, DURATION_MS, IS_DEFAULT, IS_BUNDLED, CREATED_AT, "
      "UPDATED_AT) VALUES (?, ?, ?, ?, ?, ?, ?, ?) "
      "ON CONFLICT(CHIME_ID) DO UPDATE SET DISPLAY_NAME = excluded.DISPLAY_NAME, "
      "FILE_PATH = excluded.FILE_PATH, DURATION_MS = excluded.DURATION_MS, "
      "IS_DEFAULT = excluded.IS_DEFAULT, IS_BUNDLED = excluded.IS_BUNDLED, "
      "UPDATED_AT = excluded.UPDATED_AT";
  if (sqlite3_prepare_v2(db_, sql, -1, &stmt, nullptr) != SQLITE_OK) {
    return false;
  }
  sqlite3_bind_text(stmt, 1, chime.chime_id.c_str(), -1, SQLITE_TRANSIENT);
  sqlite3_bind_text(stmt, 2, chime.display_name.c_str(), -1, SQLITE_TRANSIENT);
  sqlite3_bind_text(stmt, 3, chime.file_path.c_str(), -1, SQLITE_TRANSIENT);
  sqlite3_bind_int(stmt, 4, chime.duration_ms);
  sqlite3_bind_int(stmt, 5, chime.is_default ? 1 : 0);
  sqlite3_bind_int(stmt, 6, chime.is_bundled ? 1 : 0);
  sqlite3_bind_text(stmt, 7, now.c_str(), -1, SQLITE_TRANSIENT);
  sqlite3_bind_text(stmt, 8, now.c_str(), -1, SQLITE_TRANSIENT);
  const bool ok = sqlite3_step(stmt) == SQLITE_DONE;
  sqlite3_finalize(stmt);
  return ok;
}

bool PagingRepository::remove_chime(const std::string& chime_id) {
  std::lock_guard<std::mutex> lock(mutex_);
  sqlite3_stmt* stmt = nullptr;
  sqlite3_prepare_v2(
      db_, "DELETE FROM AUDIO_PAGING_CHIMES WHERE CHIME_ID = ? AND IFNULL(IS_BUNDLED, 0) = 0", -1,
      &stmt, nullptr);
  sqlite3_bind_text(stmt, 1, chime_id.c_str(), -1, SQLITE_TRANSIENT);
  const bool ok = sqlite3_step(stmt) == SQLITE_DONE;
  sqlite3_finalize(stmt);
  return ok;
}

bool PagingRepository::create_job(const std::string& job_id, const std::string& correlation_id,
                                  const std::string& source, const std::string& job_type,
                                  int text_length, const std::string& voice_id,
                                  const std::string& chime_id, bool include_chime) {
  std::lock_guard<std::mutex> lock(mutex_);
  sqlite3_stmt* stmt = nullptr;
  const char* sql =
      "INSERT INTO AUDIO_PAGING_JOBS "
      "(JOB_ID, CORRELATION_ID, STATUS, SOURCE, JOB_TYPE, TEXT_LENGTH, VOICE_ID, CHIME_ID, "
      "INCLUDE_CHIME, REQUESTED_AT, CREATED_AT, UPDATED_AT) "
      "VALUES (?, ?, 'requested', ?, ?, ?, ?, ?, ?, ?, ?, ?)";
  if (sqlite3_prepare_v2(db_, sql, -1, &stmt, nullptr) != SQLITE_OK) {
    return false;
  }
  const std::string now = now_utc();
  sqlite3_bind_text(stmt, 1, job_id.c_str(), -1, SQLITE_TRANSIENT);
  sqlite3_bind_text(stmt, 2, correlation_id.c_str(), -1, SQLITE_TRANSIENT);
  sqlite3_bind_text(stmt, 3, source.c_str(), -1, SQLITE_TRANSIENT);
  sqlite3_bind_text(stmt, 4, job_type.c_str(), -1, SQLITE_TRANSIENT);
  sqlite3_bind_int(stmt, 5, text_length);
  if (voice_id.empty()) {
    sqlite3_bind_null(stmt, 6);
  } else {
    sqlite3_bind_text(stmt, 6, voice_id.c_str(), -1, SQLITE_TRANSIENT);
  }
  if (chime_id.empty()) {
    sqlite3_bind_null(stmt, 7);
  } else {
    sqlite3_bind_text(stmt, 7, chime_id.c_str(), -1, SQLITE_TRANSIENT);
  }
  sqlite3_bind_int(stmt, 8, include_chime ? 1 : 0);
  sqlite3_bind_text(stmt, 9, now.c_str(), -1, SQLITE_TRANSIENT);
  sqlite3_bind_text(stmt, 10, now.c_str(), -1, SQLITE_TRANSIENT);
  sqlite3_bind_text(stmt, 11, now.c_str(), -1, SQLITE_TRANSIENT);
  const bool ok = sqlite3_step(stmt) == SQLITE_DONE;
  sqlite3_finalize(stmt);
  return ok;
}

bool PagingRepository::mark_job_completed(const std::string& job_id) {
  std::lock_guard<std::mutex> lock(mutex_);
  sqlite3_stmt* stmt = nullptr;
  const char* sql =
      "UPDATE AUDIO_PAGING_JOBS SET STATUS = 'completed', COMPLETED_AT = ?, UPDATED_AT = ? "
      "WHERE JOB_ID = ?";
  sqlite3_prepare_v2(db_, sql, -1, &stmt, nullptr);
  const std::string now = now_utc();
  sqlite3_bind_text(stmt, 1, now.c_str(), -1, SQLITE_TRANSIENT);
  sqlite3_bind_text(stmt, 2, now.c_str(), -1, SQLITE_TRANSIENT);
  sqlite3_bind_text(stmt, 3, job_id.c_str(), -1, SQLITE_TRANSIENT);
  const bool ok = sqlite3_step(stmt) == SQLITE_DONE;
  sqlite3_finalize(stmt);
  return ok;
}

bool PagingRepository::mark_job_failed(const std::string& job_id, const std::string& error_stage,
                                       const std::string& message) {
  std::lock_guard<std::mutex> lock(mutex_);
  sqlite3_stmt* stmt = nullptr;
  const char* sql =
      "UPDATE AUDIO_PAGING_JOBS SET STATUS = 'failed', ERROR_STAGE = ?, ERROR_MESSAGE = ?, "
      "COMPLETED_AT = ?, UPDATED_AT = ? WHERE JOB_ID = ?";
  sqlite3_prepare_v2(db_, sql, -1, &stmt, nullptr);
  const std::string now = now_utc();
  sqlite3_bind_text(stmt, 1, error_stage.c_str(), -1, SQLITE_TRANSIENT);
  sqlite3_bind_text(stmt, 2, message.c_str(), -1, SQLITE_TRANSIENT);
  sqlite3_bind_text(stmt, 3, now.c_str(), -1, SQLITE_TRANSIENT);
  sqlite3_bind_text(stmt, 4, now.c_str(), -1, SQLITE_TRANSIENT);
  sqlite3_bind_text(stmt, 5, job_id.c_str(), -1, SQLITE_TRANSIENT);
  const bool ok = sqlite3_step(stmt) == SQLITE_DONE;
  sqlite3_finalize(stmt);
  return ok;
}

PagingApiKeyMetadata PagingRepository::get_api_key() const {
  std::lock_guard<std::mutex> lock(mutex_);
  PagingApiKeyMetadata metadata;
  sqlite3_stmt* stmt = nullptr;
  if (sqlite3_prepare_v2(db_, "SELECT KEY_HASH, KEY_PREFIX FROM AUDIO_PAGING_API_KEY WHERE ID = 1",
                         -1, &stmt, nullptr) != SQLITE_OK) {
    return metadata;
  }
  if (sqlite3_step(stmt) == SQLITE_ROW) {
    if (const char* hash = reinterpret_cast<const char*>(sqlite3_column_text(stmt, 0))) {
      metadata.hash = hash;
      metadata.configured = metadata.hash.size() > 0;
    }
    if (const char* prefix = reinterpret_cast<const char*>(sqlite3_column_text(stmt, 1))) {
      metadata.prefix = prefix;
    }
  }
  sqlite3_finalize(stmt);
  return metadata;
}

bool PagingRepository::set_api_key(const std::string& key_hash, const std::string& key_prefix) {
  std::lock_guard<std::mutex> lock(mutex_);
  const std::string now = now_utc();
  sqlite3_stmt* stmt = nullptr;
  const char* sql =
      "INSERT INTO AUDIO_PAGING_API_KEY (ID, KEY_HASH, KEY_PREFIX, CREATED_AT, UPDATED_AT) "
      "VALUES (1, ?, ?, ?, ?) "
      "ON CONFLICT(ID) DO UPDATE SET KEY_HASH = excluded.KEY_HASH, KEY_PREFIX = excluded.KEY_PREFIX, "
      "UPDATED_AT = excluded.UPDATED_AT";
  if (sqlite3_prepare_v2(db_, sql, -1, &stmt, nullptr) != SQLITE_OK) {
    return false;
  }
  sqlite3_bind_text(stmt, 1, key_hash.c_str(), -1, SQLITE_TRANSIENT);
  sqlite3_bind_text(stmt, 2, key_prefix.c_str(), -1, SQLITE_TRANSIENT);
  sqlite3_bind_text(stmt, 3, now.c_str(), -1, SQLITE_TRANSIENT);
  sqlite3_bind_text(stmt, 4, now.c_str(), -1, SQLITE_TRANSIENT);
  const bool ok = sqlite3_step(stmt) == SQLITE_DONE;
  sqlite3_finalize(stmt);
  return ok;
}

bool PagingRepository::clear_api_key() {
  std::lock_guard<std::mutex> lock(mutex_);
  return exec_locked("DELETE FROM AUDIO_PAGING_API_KEY WHERE ID = 1");
}

int PagingRepository::prune_old_jobs(int retention_days) {
  std::lock_guard<std::mutex> lock(mutex_);
  std::ostringstream sql;
  sql << "DELETE FROM AUDIO_PAGING_JOBS WHERE datetime(CREATED_AT) < datetime('now', '-"
      << retention_days << " days')";
  if (!exec_locked(sql.str())) {
    return 0;
  }
  return sqlite3_changes(db_);
}

}  // namespace homepi::audio_paging
