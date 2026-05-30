#include "homepi/shairport-sync/supervisor.hpp"

#include <chrono>

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
  cv_.notify_one();
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
  std::vector<int> zones;
  for (const auto& zone : db_.get_zones()) {
    if (zone.enabled.value_or(1) != 0) {
      zones.push_back(zone.zone_number);
    }
  }
  if (zones.empty()) {
    for (int zone = 1; zone <= config_.zone_count; ++zone) {
      zones.push_back(zone);
    }
  }
  return zones;
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

  const bool was_running = state_ == SupervisorState::running;
  state_ = SupervisorState::configuring;
  failed_prerequisites_.clear();

  const auto zones = db_.get_zones();
  const auto settings = db_.get_zone_settings();
  const auto new_hashes = generator_.generate(zones, settings, *airplay_source);

  std::vector<int> restart_zones;
  for (const auto& [zone, hash] : new_hashes) {
    const auto existing = config_hashes_.find(zone);
    if (existing == config_hashes_.end() || existing->second != hash) {
      restart_zones.push_back(zone);
    }
  }

  config_hashes_ = new_hashes;

  const auto enabled = enabled_zone_numbers();
  if (!was_running) {
    systemd_.start_zones(enabled);
    state_ = SupervisorState::running;
    active_zone_count_ = static_cast<int>(enabled.size());
    return;
  }

  if (!restart_zones.empty()) {
    systemd_.restart_zones(restart_zones);
  }
  active_zone_count_ = static_cast<int>(enabled.size());
  state_ = SupervisorState::running;
}

void Supervisor::run() {
  hifi_client_.start(config_.hifi_socket_path, [this](const std::string& line) {
    if (line.find("audio_state_snapshot") != std::string::npos) {
      request_evaluate();
    }
  });

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
