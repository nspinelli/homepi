#pragma once

#include <mutex>
#include <optional>
#include <string>
#include <vector>

#include "homepi/audio-paging/types.hpp"

struct sqlite3;

namespace homepi::audio_paging {

/** SQLite-backed repository for paging config, voices, chimes, and job rows. */
class PagingRepository {
 public:
  /**
   * Opens the database connection and applies SQL migrations.
   * @param database_path SQLite database file.
   * @param migration_sql Full SQL migration script.
   */
  PagingRepository(std::string database_path, std::string migration_sql);
  ~PagingRepository();

  PagingRepository(const PagingRepository&) = delete;
  PagingRepository& operator=(const PagingRepository&) = delete;

  /** Returns paging configuration row. */
  PagingConfig get_config() const;

  /** Updates mutable paging configuration fields. */
  bool update_config(const PagingConfig& config);

  /** Returns installed voice rows. */
  std::vector<PagingVoice> list_voices() const;

  /** Returns installed chime rows. */
  std::vector<PagingChime> list_chimes() const;

  /** Returns one voice row by id. */
  std::optional<PagingVoice> get_voice(const std::string& voice_id) const;

  /** Returns one chime row by id. */
  std::optional<PagingChime> get_chime(const std::string& chime_id) const;

  /** Sets active voice and updates timestamp. */
  bool set_active_voice(const std::string& voice_id);

  /** Sets active chime. */
  bool set_active_chime(const std::string& chime_id);

  /** Inserts or updates a voice row. */
  bool upsert_voice(const PagingVoice& voice);

  /** Removes non-bundled voice row by id. */
  bool remove_voice(const std::string& voice_id);

  /** Inserts or updates a chime row. */
  bool upsert_chime(const PagingChime& chime);

  /** Removes non-bundled chime row by id. */
  bool remove_chime(const std::string& chime_id);

  /** Inserts a job row for tracking lifecycle. */
  bool create_job(const std::string& job_id, const std::string& correlation_id,
                  const std::string& source, const std::string& job_type, int text_length,
                  const std::string& voice_id, const std::string& chime_id, bool include_chime);

  /** Marks a job as completed with timestamps. */
  bool mark_job_completed(const std::string& job_id);

  /** Marks a job as failed with stage and reason. */
  bool mark_job_failed(const std::string& job_id, const std::string& error_stage,
                       const std::string& message);

  /** Returns stored API key metadata. */
  PagingApiKeyMetadata get_api_key() const;

  /** Stores a new API key hash and prefix. */
  bool set_api_key(const std::string& key_hash, const std::string& key_prefix);

  /** Clears configured API key metadata. */
  bool clear_api_key();

  /** Deletes job rows older than the retention window. */
  int prune_old_jobs(int retention_days = 30);

 private:
  static std::string now_utc();
  bool exec_locked(const std::string& sql) const;

  std::string database_path_;
  mutable std::mutex mutex_;
  sqlite3* db_ = nullptr;
};

}  // namespace homepi::audio_paging
