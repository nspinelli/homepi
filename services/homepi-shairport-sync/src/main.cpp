#include <atomic>
#include <csignal>
#include <filesystem>
#include <fstream>
#include <iostream>
#include <sstream>
#include <thread>

#include "homepi/log.hpp"
#include "homepi/shairport-sync/config-loader.hpp"
#include "homepi/shairport-sync/db-repository.hpp"
#include "homepi/shairport-sync/hifi-event-client.hpp"
#include "homepi/shairport-sync/supervisor.hpp"

namespace {

std::atomic<bool> g_running{true};

void handle_signal(int /*signal*/) { g_running = false; }

std::string read_migration(const std::string& path) {
  std::ifstream input(path);
  std::ostringstream buffer;
  buffer << input.rdbuf();
  return buffer.str();
}

}  // namespace

int main(int argc, char* argv[]) {
  std::string config_path = "config/service-config.json";
  if (argc > 1) {
    config_path = argv[1];
  }

  std::signal(SIGINT, handle_signal);
  std::signal(SIGTERM, handle_signal);

  homepi::shairport_sync::ServiceConfig config;
  try {
    config = homepi::shairport_sync::load_service_config(config_path);
  } catch (const std::exception& ex) {
    std::cerr << "config load failed: " << ex.what() << "\n";
    return 1;
  }

  const auto log_level = config.log_level == "DEBUG"   ? homepi::logging::LogLevel::DEBUG
                         : config.log_level == "WARN" ? homepi::logging::LogLevel::WARN
                         : config.log_level == "ERROR"
                             ? homepi::logging::LogLevel::ERROR
                             : homepi::logging::LogLevel::INFO;

  homepi::logging::Logger logger(config.service, log_level);
  logger.log(log_level, "core.runtime", "lifecycle_starting", "startup",
             "homepi-shairport-supervisor starting");

  std::error_code ec;
  std::filesystem::create_directories(config.zones_config_dir, ec);
  std::filesystem::create_directories(config.hooks_dir, ec);
  std::filesystem::create_directories(config.socket_dir, ec);

  const std::string migration_path = "storage/migrations/003-shairport-sync.sql";
  homepi::shairport_sync::DbRepository db(config.database_path, read_migration(migration_path));
  homepi::shairport_sync::Supervisor supervisor(config, db);

  std::thread supervisor_thread([&]() { supervisor.run(); });

  while (g_running.load()) {
    std::this_thread::sleep_for(std::chrono::seconds(1));
  }

  supervisor.stop();
  supervisor_thread.join();

  logger.log(log_level, "core.runtime", "lifecycle_stopping", "shutdown",
             "homepi-shairport-supervisor stopping");
  return 0;
}
