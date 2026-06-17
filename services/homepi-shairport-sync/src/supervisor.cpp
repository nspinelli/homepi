#include "homepi/shairport-sync/supervisor.hpp"

#include <algorithm>
#include <chrono>
#include <filesystem>

#include "homepi/log.hpp"

namespace homepi::shairport_sync {

Supervisor::Supervisor(ServiceConfig config, DbRepository& db)
    : config_(std::move(config)),
      db_(db),
      generator_(config_) {}

void Supervisor::request_evaluate() {
  evaluate_pending_ = true;
  cv_.notify_one();
}

void Supervisor::stop() {
  running_ = false;
  hifi_client_.stop();
  usb_client_.stop();
  cv_.notify_one();
}

homepi::storage::AudioProfileTuple Supervisor::load_loopback_profile() const {
  if (std::filesystem::exists(config_.database_path)) {
    try {
      homepi::storage::DatabaseConnection db(config_.database_path,
                                             homepi::storage::DatabaseOpenMode::ReadOnly);
      homepi::storage::AudioProfileRepository repo(db);
      return repo.load_active_config().loopback_profile;
    } catch (...) {
    }
  }

  const auto artifact =
      homepi::storage::AudioProfileRepository::load_from_artifact(config_.artifact_path);
  if (artifact.has_value()) {
    return artifact->loopback_profile;
  }
  return homepi::storage::platform_loopback_default();
}

SupervisorHealth Supervisor::health() const {
  std::lock_guard lock(mutex_);
  SupervisorHealth health;
  health.state = state_;
  health.failed_prerequisites = failed_prerequisites_;
  health.active_zone_count = active_zone_count_;
  return health;
}

void Supervisor::set_airplay_source(int source_number) {
  db_.set_airplay_source(source_number);
  request_evaluate();
}

std::vector<int> Supervisor::enabled_zone_numbers() const {
  const auto rows = db_.get_zones();
  std::vector<int> zones;
  zones.reserve(rows.size());
  for (const auto& zone : rows) {
    if (zone.enabled.has_value() && zone.enabled.value() == 1) {
      zones.push_back(zone.zone_number);
    }
  }
  return zones;
}

void Supervisor::reconcile_zone_units(const std::vector<int>& enabled) {
  std::vector<int> start_zones;
  std::vector<int> stop_zones;
  for (int zone = 1; zone <= config_.zone_count; ++zone) {
    const bool should_run =
        std::find(enabled.begin(), enabled.end(), zone) != enabled.end();
    const std::string unit = "homepi-shairport@" + std::to_string(zone) + ".service";
    if (should_run && !systemd_.is_unit_active(unit)) {
      start_zones.push_back(zone);
    } else if (!should_run && systemd_.is_unit_active(unit)) {
      stop_zones.push_back(zone);
    }
  }
  if (!start_zones.empty()) {
    systemd_.start_zones(start_zones);
  }
  if (!stop_zones.empty()) {
    systemd_.stop_zones(stop_zones);
  }
}

void Supervisor::transition_offline(const std::vector<std::string>& failures) {
  systemd_.stop_all_zones(config_.zone_count);
  state_ = SupervisorState::offline;
  failed_prerequisites_ = failures;
  active_zone_count_ = 0;
  config_hashes_.clear();
}

void Supervisor::evaluate() {
  const ReadinessResult gate = gate_.evaluate(db_);
  if (!gate.ready) {
    if (state_ == SupervisorState::running) {
      failed_prerequisites_ = gate.failures;
      return;
    }
    transition_offline(gate.failures);
    return;
  }

  const auto airplay_source = db_.airplay_source_number();
  if (!airplay_source.has_value()) {
    if (state_ == SupervisorState::running) {
      failed_prerequisites_ = {"airplay source not configured"};
      return;
    }
    transition_offline({"airplay source not configured"});
    return;
  }

  state_ = SupervisorState::configuring;
  failed_prerequisites_.clear();

  const auto zones = db_.get_zones();
  const auto settings = db_.get_zone_settings();
  const auto loopback_profile = load_loopback_profile();
  const auto new_hashes =
      generator_.generate(zones, settings, *airplay_source, loopback_profile);

  std::vector<int> restart_zones;
  for (const auto& [zone, hash] : new_hashes) {
    const auto existing = config_hashes_.find(zone);
    if (existing == config_hashes_.end() || existing->second != hash) {
      restart_zones.push_back(zone);
    }
  }

  config_hashes_ = new_hashes;

  const auto enabled = enabled_zone_numbers();
  if (!restart_zones.empty()) {
    systemd_.restart_zones(restart_zones);
  }
  reconcile_zone_units(enabled);

  active_zone_count_ = static_cast<int>(enabled.size());
  state_ = SupervisorState::running;
}

void Supervisor::run() {
  hifi_client_.start(config_.hifi_socket_path, [this](const std::string& line) {
    if (line.find("audio_state_snapshot") != std::string::npos ||
        line.find("zone_enable_changed") != std::string::npos) {
      request_evaluate();
    }
  });

  usb_client_.start(config_.usb_devices_socket_path, [this](const std::string& line) {
    if (line.find("audio_operating_profile_changed") != std::string::npos ||
        line.find("audio_profile_paused") != std::string::npos ||
        line.find("audio_profile_invalid") != std::string::npos ||
        line.find("primary_audio_unassigned") != std::string::npos) {
      request_evaluate();
    }
  });

  {
    std::lock_guard lock(mutex_);
    evaluate();
  }

  while (running_.load()) {
    if (evaluate_pending_.exchange(false)) {
      {
        std::lock_guard lock(mutex_);
        evaluate();
      }
    }

    std::unique_lock lock(mutex_);
    cv_.wait_for(lock, std::chrono::seconds(config_.health_interval_sec), [this]() {
      return !running_.load() || evaluate_pending_.load();
    });

    if (!running_.load()) {
      break;
    }

    if (!evaluate_pending_.load()) {
      evaluate_pending_ = true;
    }
  }

  systemd_.stop_all_zones(config_.zone_count);
}

}  // namespace homepi::shairport_sync
