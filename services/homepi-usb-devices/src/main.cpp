#include <atomic>
#include <chrono>
#include <csignal>
#include <filesystem>
#include <fstream>
#include <iostream>
#include <mutex>
#include <sstream>
#include <string>
#include <thread>

#include "homepi/log.hpp"
#include "homepi/usb-devices/artifact-writer.hpp"
#include "homepi/usb-devices/assignment-repository.hpp"
#include "homepi/usb-devices/config-loader.hpp"
#include "homepi/usb-devices/device-scanner.hpp"
#include "homepi/usb-devices/udev-monitor.hpp"
#include "homepi/usb-devices/service-health-emitter.hpp"
#include "homepi/usb-devices/unix-api-server.hpp"

namespace {

std::atomic<bool> g_running{true};
std::mutex g_state_mutex;
homepi::usb_devices::ServiceHealth g_health;
homepi::usb_devices::AssignmentRepository* g_repository = nullptr;
homepi::usb_devices::ArtifactWriter* g_artifacts = nullptr;
homepi::usb_devices::UnixApiServer* g_server = nullptr;
homepi::usb_devices::ServiceConfig g_config;

void emit_health_event(const std::string& event_name, const std::string& correlation_id) {
  if (g_server == nullptr) {
    return;
  }
  homepi::usb_devices::ServiceHealth health;
  {
    std::lock_guard lock(g_state_mutex);
    health = g_health;
  }
  homepi::usb_devices::emit_service_health(*g_server, health, event_name, correlation_id);
}

/**
 * Handles termination signals.
 * @param signal Signal number.
 */
void handle_signal(int /*signal*/) { g_running = false; }

/**
 * Reads a migration SQL file from disk.
 * @param path File path.
 * @return SQL contents.
 */
std::string read_migration(const std::string& path) {
  std::ifstream input(path);
  std::ostringstream buffer;
  buffer << input.rdbuf();
  return buffer.str();
}

/**
 * Refreshes device inventory and regenerates artifacts.
 */
void refresh_devices() {
  if (g_repository == nullptr || g_artifacts == nullptr) {
    return;
  }
  const auto scanned = homepi::usb_devices::scan_usb_devices();
  g_repository->upsert_devices(scanned);
  const auto assignments = g_repository->get_assignments();
  const auto devices = g_repository->list_devices();
  g_artifacts->regenerate(assignments, devices);

  std::lock_guard lock(g_state_mutex);
  g_health.connected_device_count = static_cast<int>(scanned.size());
  g_health.assignments_degraded =
      homepi::usb_devices::AssignmentRepository::assignments_degraded(assignments, devices);
  g_health.last_scan_at = "now";
  emit_health_event(
      g_health.assignments_degraded ? "assignments_degraded" : "assignments_recovered", "hotplug");
}

}  // namespace

int main(int argc, char* argv[]) {
  std::string config_path = "config/service-config.json";
  if (argc > 1) {
    config_path = argv[1];
  }

  std::signal(SIGINT, handle_signal);
  std::signal(SIGTERM, handle_signal);

  try {
    g_config = homepi::usb_devices::load_service_config(config_path);
  } catch (const std::exception& ex) {
    std::cerr << "config load failed: " << ex.what() << "\n";
    return 1;
  }

  const auto log_level = g_config.log_level == "DEBUG"   ? homepi::logging::LogLevel::DEBUG
                         : g_config.log_level == "WARN" ? homepi::logging::LogLevel::WARN
                         : g_config.log_level == "ERROR"
                             ? homepi::logging::LogLevel::ERROR
                             : homepi::logging::LogLevel::INFO;

  homepi::logging::Logger logger(g_config.service, log_level);
  logger.log(log_level, "core.runtime", "lifecycle_starting", "startup",
             "homepi-usb-devices starting");

  const std::string migration_path = "storage/migrations/001-usb-devices.sql";
  {
    std::error_code ec;
    std::filesystem::create_directories(std::filesystem::path(g_config.database_path).parent_path(),
                                      ec);
    std::filesystem::create_directories(g_config.generated_dir, ec);
    std::filesystem::create_directories(g_config.socket_dir, ec);
  }

  homepi::usb_devices::AssignmentRepository repository(g_config.database_path,
                                                       read_migration(migration_path));
  homepi::usb_devices::ArtifactWriter artifacts(g_config);
  g_repository = &repository;
  g_artifacts = &artifacts;

  refresh_devices();

  homepi::usb_devices::UdevMonitor monitor;
  monitor.start([&logger](const std::string& action, const std::string& /*devpath*/) {
    if (action == "add" || action == "remove") {
      refresh_devices();
      emit_health_event(action == "add" ? "device_added" : "device_removed", "hotplug");
      logger.log(homepi::logging::LogLevel::INFO, "usb.devices", "device_hotplug", "hotplug",
                 "USB hotplug refresh", std::string("{\"action\":\"") + action + "\"}");
    }
  });

  {
    std::lock_guard lock(g_state_mutex);
    g_health.lifecycle = "running";
    g_health.udev_monitor_active = monitor.active();
    g_health.last_scan_at = "now";
  }

  homepi::usb_devices::UnixApiServer server(homepi::usb_devices::ApiContext{
      .repository = &repository,
      .artifacts = &artifacts,
      .config = g_config,
      .scan_fn = homepi::usb_devices::scan_usb_devices,
      .on_devices_changed = refresh_devices,
      .health_fn = []() {
        std::lock_guard lock(g_state_mutex);
        return g_health;
      },
      .on_subscribe_fn = [&]() { emit_health_event("service_ready", "subscribe"); },
  });

  g_server = &server;

  if (!server.start()) {
    logger.log(homepi::logging::LogLevel::ERROR, "usb.api", "socket_bind_failed", "startup",
               "Failed to bind unix socket");
    return 1;
  }

  logger.log(homepi::logging::LogLevel::INFO, "core.runtime", "service_started", "startup",
             "homepi-usb-devices running",
             std::string("{\"socketPath\":\"") + g_config.socket_path + "\"}");
  emit_health_event("service_started", "startup");

  while (g_running.load()) {
    std::this_thread::sleep_for(std::chrono::seconds(1));
    std::lock_guard lock(g_state_mutex);
    g_health.udev_monitor_active = monitor.active();
  }

  logger.log(homepi::logging::LogLevel::INFO, "core.runtime", "lifecycle_stopping", "shutdown",
             "homepi-usb-devices stopping");
  server.stop();
  monitor.stop();
  return 0;
}
