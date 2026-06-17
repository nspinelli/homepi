#include <cassert>
#include <filesystem>
#include <fstream>
#include <iostream>
#include <sstream>
#include <string>

#include <sqlite3.h>

#include "homepi/storage/audio-profile-repository.hpp"
#include "homepi/storage/database-connection.hpp"
#include "homepi/storage/migration-runner.hpp"

namespace {

std::string read_file(const std::string& path) {
  std::ifstream input(path);
  std::ostringstream buffer;
  buffer << input.rdbuf();
  return buffer.str();
}

void seed_platform_profile(homepi::storage::DatabaseConnection& db) {
  sqlite3_exec(db.handle(),
               "INSERT OR REPLACE INTO audio_operating_profiles "
               "(role, device_id, alsa_device, sample_rate, channels, sample_format, profile_source, "
               "updated_at) VALUES ('platform_loopback', NULL, NULL, 44100, 2, 'S16_LE', "
               "'platform_policy', datetime('now'));",
               nullptr, nullptr, nullptr);
}

}  // namespace

int main() {
  const std::string db_path = "/tmp/homepi-audio-profile-test.sqlite";
  std::filesystem::remove(db_path);

  homepi::storage::DatabaseConnection db(db_path, homepi::storage::DatabaseOpenMode::ReadWrite);
  const std::string migration_001 =
      read_file("services/homepi-usb-devices/storage/migrations/001-usb-devices.sql");
  const std::string migration_002 =
      read_file("services/homepi-usb-devices/storage/migrations/002-audio-profiles.sql");
  homepi::storage::MigrationRunner::apply(db, migration_001);
  homepi::storage::MigrationRunner::apply(db, migration_002);
  seed_platform_profile(db);

  homepi::storage::AudioProfileRepository repo(db);
  const homepi::storage::ActiveAudioConfig config = repo.load_active_config();
  assert(config.mode == homepi::storage::ProfileMode::NoDacAssigned);
  assert(config.loopback_profile.sample_rate == 44100);
  assert(config.loopback_profile.channels == 2);
  assert(config.loopback_profile.sample_format == homepi::storage::SampleFormat::S16Le);

  std::cout << "test_audio_profile_repository: OK\n";
  return 0;
}
