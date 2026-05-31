#include <atomic>
#include <chrono>
#include <csignal>
#include <cerrno>
#include <cstring>
#include <fcntl.h>
#include <sys/file.h>
#include <filesystem>
#include <fstream>
#include <iostream>
#include <mutex>
#include <sstream>
#include <string>
#include <thread>
#include <unistd.h>

#include "homepi/log.hpp"
#include "homepi/hifi-serial/command-queue.hpp"
#include "homepi/hifi-serial/config-loader.hpp"
#include "homepi/hifi-serial/event-publisher.hpp"
#include "homepi/hifi-serial/json-utils.hpp"
#include "homepi/hifi-serial/protocol-encoder.hpp"
#include "homepi/hifi-serial/protocol-parser.hpp"
#include "homepi/hifi-serial/serial-path-resolver.hpp"
#include "homepi/hifi-serial/serial-port.hpp"
#include "homepi/hifi-serial/state-repository.hpp"
#include "homepi/hifi-serial/sync-engine.hpp"
#include "homepi/hifi-serial/service-health-emitter.hpp"
#include "homepi/hifi-serial/unix-api-server.hpp"

namespace {

std::atomic<bool> g_running{true};
std::mutex g_health_mutex;
homepi::hifi_serial::ServiceHealth g_health;
homepi::hifi_serial::StateRepository* g_repository = nullptr;
homepi::hifi_serial::CommandQueue* g_queue = nullptr;
homepi::hifi_serial::UnixApiServer* g_server = nullptr;
homepi::hifi_serial::EventPublisher* g_events = nullptr;

void handle_signal(int /*signal*/) { g_running = false; }

void emit_health_event(const std::string& event_name, const std::string& correlation_id) {
  if (g_server == nullptr) {
    return;
  }
  homepi::hifi_serial::ServiceHealth health;
  {
    std::lock_guard lock(g_health_mutex);
    health = g_health;
    if (g_queue) {
      health.queue_depth = g_queue->pending_count();
    }
  }
  homepi::hifi_serial::emit_service_health(*g_server, health, event_name, correlation_id);
}

std::string read_migration(const std::string& path) {
  std::ifstream input(path);
  std::ostringstream buffer;
  buffer << input.rdbuf();
  return buffer.str();
}

/**
 * Ensures only one daemon instance runs (prevents duplicate serial/socket contention).
 * @param lock_path Path to the flock lock file.
 * @returns True when this process acquired the lock.
 */
bool acquire_instance_lock(const std::string& lock_path) {
  const int fd = ::open(lock_path.c_str(), O_RDWR | O_CREAT, 0644);
  if (fd < 0) {
    return false;
  }
  if (::flock(fd, LOCK_EX | LOCK_NB) != 0) {
    ::close(fd);
    return false;
  }
  return true;
}

}  // namespace

int main(int argc, char* argv[]) {
  std::string config_path = "config/service-config.json";
  if (argc > 1) {
    config_path = argv[1];
  }

  std::signal(SIGINT, handle_signal);
  std::signal(SIGTERM, handle_signal);

  homepi::hifi_serial::ServiceConfig config;
  try {
    config = homepi::hifi_serial::load_service_config(config_path);
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
             "homepi-hifi-serial starting");

  const std::string instance_lock_path = config.socket_dir + "/hifi-serial.lock";
  if (!acquire_instance_lock(instance_lock_path)) {
    logger.log(homepi::logging::LogLevel::ERROR, "core.runtime", "instance_already_running",
               "startup", "Another homepi-hifi-serial instance is already running",
               std::string("{\"lockPath\":\"") + instance_lock_path + "\"}");
    return 1;
  }

  const std::string migration_path = "storage/migrations/002-hifi-serial.sql";
  {
    std::error_code ec;
    std::filesystem::create_directories(
        std::filesystem::path(config.database_path).parent_path(), ec);
    std::filesystem::create_directories(config.socket_dir, ec);
  }

  homepi::hifi_serial::StateRepository repository(config.database_path,
                                                  read_migration(migration_path));
  {
    const std::string shairport_migration = read_migration("storage/migrations/003-shairport-sync.sql");
    if (!shairport_migration.empty()) {
      try {
        repository.apply_migration(shairport_migration);
      } catch (const std::exception& ex) {
        logger.log(homepi::logging::LogLevel::WARN, "hifi.serial", "shairport_migration_failed",
                   "startup", ex.what());
      }
    }
  }
  g_repository = &repository;

  const auto serial_resolution = homepi::hifi_serial::resolve_serial_path(
      config.database_path, config.virtual_port);
  const std::string& serial_path = serial_resolution.path;

  homepi::hifi_serial::SerialPort port;
  homepi::hifi_serial::CommandQueue queue(port, config.command_interval_ms);
  g_queue = &queue;

  homepi::hifi_serial::UnixApiServer server(homepi::hifi_serial::ApiContext{
      .config = config,
      .health_fn = []() {
        std::lock_guard lock(g_health_mutex);
        if (g_queue) {
          g_health.queue_depth = g_queue->pending_count();
        }
        return g_health;
      },
      .handle_rpc_fn = [&](const std::string& line) -> std::string {
        const std::string method = homepi::hifi_serial::json_get_string(line, "method");
        const std::string correlation_id =
            homepi::hifi_serial::json_get_string(line, "correlationId");

        auto ok = [&](const std::string& data) {
          std::ostringstream out;
          out << "{\"ok\":true,\"correlationId\":\""
              << homepi::hifi_serial::json_escape(
                     correlation_id.empty() ? "hifi-serial" : correlation_id)
              << "\",\"data\":" << data << "}";
          return out.str();
        };
        auto err = [&](const std::string& message) {
          std::ostringstream out;
          out << "{\"ok\":false,\"correlationId\":\""
              << homepi::hifi_serial::json_escape(
                     correlation_id.empty() ? "hifi-serial" : correlation_id)
              << "\",\"error\":{\"code\":\"HIFI_SERIAL_ERROR\",\"message\":\""
              << homepi::hifi_serial::json_escape(message) << "\"}}";
          return out.str();
        };

        if (method == "getHealth") {
          homepi::hifi_serial::ServiceHealth health;
          {
            std::lock_guard lock(g_health_mutex);
            health = g_health;
          }
          std::ostringstream data;
          data << "{"
               << "\"lifecycle\":\"" << homepi::hifi_serial::json_escape(health.lifecycle)
               << "\","
               << "\"connected\":" << (health.connected ? "true" : "false") << ","
               << "\"serialPath\":"
               << (health.serial_path.empty()
                       ? "null"
                       : "\"" + homepi::hifi_serial::json_escape(health.serial_path) + "\"")
               << ","
               << "\"serialAssigned\":" << (health.serial_assigned ? "true" : "false") << ","
               << "\"syncInProgress\":" << (health.sync_in_progress ? "true" : "false") << ","
               << "\"degraded\":" << (health.degraded ? "true" : "false") << ","
               << "\"lastFullSyncAt\":"
               << (health.last_full_sync_at.empty()
                       ? "null"
                       : "\"" + homepi::hifi_serial::json_escape(health.last_full_sync_at) +
                             "\"")
               << ",\"queueDepth\":" << health.queue_depth << "}";
          return ok(data.str());
        }

        if (method == "syncController") {
          {
            std::lock_guard lock(g_health_mutex);
            g_health.sync_in_progress = true;
          }
          emit_health_event("sync_started", correlation_id);
          homepi::hifi_serial::SyncEngine::run_full_sync(queue);
          repository.mark_full_sync_complete();
          {
            std::lock_guard lock(g_health_mutex);
            g_health.sync_in_progress = false;
            g_health.last_full_sync_at = "now";
            g_health.degraded = false;
          }
          emit_health_event("sync_completed", correlation_id);
          emit_health_event("service_recovered", correlation_id);
          return ok("{\"synced\":true}");
        }

        if (method == "getSnapshot") {
          return ok(repository.snapshot_json());
        }

        if (method == "getController") {
          return ok(repository.controller_json());
        }

        if (method == "getZones") {
          return ok("{\"zones\":" + repository.zones_json() + "}");
        }

        if (method == "getSources") {
          return ok("{\"sources\":" + repository.sources_json() + "}");
        }

        if (method == "getGroups") {
          return ok("{\"groups\":" + repository.groups_json() + "}");
        }

        if (method == "getLanguageStrings") {
          std::ostringstream out;
          out << "{\"languageStrings\":[";
          const auto strings = repository.get_language_strings();
          for (std::size_t i = 0; i < strings.size(); ++i) {
            if (i > 0) {
              out << ",";
            }
            out << "{\"stringNumber\":" << strings[i].string_number;
            if (strings[i].value) {
              out << ",\"value\":\"" << homepi::hifi_serial::json_escape(*strings[i].value) << "\"";
            }
            out << "}";
          }
          out << "]}";
          return ok(out.str());
        }

        if (method == "sendCommand") {
          const std::string command = homepi::hifi_serial::json_get_string(line, "command");
          if (command.empty()) {
            return err("command required");
          }
          queue.enqueue(homepi::hifi_serial::cmd_raw(command));
          return ok("{\"queued\":true}");
        }

        if (method == "patchZoneController") {
          const int zone_number = homepi::hifi_serial::json_get_int(line, "zoneNumber");
          if (zone_number < 1 || zone_number > 16) {
            return err("zoneNumber must be between 1 and 16");
          }
          repository.patch_zone_controller(zone_number, line);
          return ok("{\"patched\":true}");
        }

        if (method == "getAirplaySource") {
          const auto source = repository.airplay_source_number();
          if (!source.has_value()) {
            return ok("{\"sourceNumber\":null}");
          }
          return ok("{\"sourceNumber\":" + std::to_string(*source) + "}");
        }

        if (method == "getShairportZoneSettings") {
          return ok("{\"shairportZoneSettings\":" + repository.shairport_zone_settings_json() +
                    "}");
        }

        if (method == "updateShairportZoneSettings") {
          const int zone_number = homepi::hifi_serial::json_get_int(line, "zoneNumber");
          if (zone_number < 1 || zone_number > 16) {
            return err("zoneNumber must be between 1 and 16");
          }
          const std::string profile =
              homepi::hifi_serial::json_get_string(line, "volumeControlProfile");
          const double active_timeout =
              homepi::hifi_serial::json_get_double(line, "activeStateTimeout");
          const int session_timeout =
              homepi::hifi_serial::json_get_int(line, "sessionTimeout");
          const int log_verbosity = homepi::hifi_serial::json_get_int(line, "logVerbosity");
          repository.update_shairport_zone_settings(
              zone_number, profile,
              active_timeout >= 0 ? active_timeout : -1.0,
              session_timeout >= 0 ? session_timeout : -1,
              log_verbosity >= 0 ? log_verbosity : -1);
          return ok("{\"updated\":true}");
        }

        if (method == "setAirplaySource") {
          const auto colon = line.find("\"sourceNumber\"");
          if (colon == std::string::npos) {
            return err("sourceNumber required");
          }
          const auto value_colon = line.find(':', colon);
          if (value_colon == std::string::npos) {
            return err("sourceNumber required");
          }
          int source_number = 0;
          try {
            source_number = std::stoi(line.substr(value_colon + 1));
          } catch (...) {
            return err("sourceNumber must be an integer");
          }
          if (source_number < 1 || source_number > 8) {
            return err("sourceNumber must be between 1 and 8");
          }
          repository.set_airplay_source(source_number);
          return ok("{\"sourceNumber\":" + std::to_string(source_number) + "}");
        }

        if (method == "subscribe") {
          return ok("{\"subscribed\":true}");
        }

        return err("unknown method");
      },
      .snapshot_json_fn = [&]() { return repository.snapshot_json(); },
      .on_subscribe_fn = [&]() { emit_health_event("service_ready", "subscribe"); },
  });

  g_server = &server;

  homepi::hifi_serial::EventPublisher events([&](const std::string& line) {
    if (g_server) {
      g_server->broadcast(line);
    }
  });
  g_events = &events;

  port.set_line_callback([&](const std::string& line) {
    const auto updates = homepi::hifi_serial::parse_response_line(line);
    for (const auto& update : updates) {
      repository.apply_parsed_update(update);
      if (g_events) {
        g_events->publish(update, "serial");
      }
    }
  });

  if (!server.start()) {
    logger.log(homepi::logging::LogLevel::ERROR, "hifi.api", "socket_bind_failed", "startup",
               "Failed to bind unix socket",
               std::string("{\"socketPath\":\"") + config.socket_path + "\",\"errno\":" +
                   std::to_string(errno) + ",\"error\":\"" +
                   homepi::hifi_serial::json_escape(std::strerror(errno)) + "\"}");
    return 1;
  }

  logger.log(log_level, "core.runtime", "service_started", "startup",
             "homepi-hifi-serial running",
             std::string("{\"socketPath\":\"") + config.socket_path + "\"}");
  emit_health_event("service_started", "startup");

  const bool serial_opened =
      !serial_path.empty() && port.open(serial_path, config.baud_rate);
  if (serial_opened) {
    {
      std::lock_guard lock(g_health_mutex);
      g_health.connected = true;
      g_health.serial_assigned = true;
      g_health.serial_path = serial_path;
      g_health.lifecycle = "running";
    }
    repository.set_serial_metadata(serial_resolution.device_id, serial_path);
    logger.log(log_level, "hifi.serial", "serial_connected", "startup",
               "Serial port opened", "{\"path\":\"" + serial_path + "\"}");
    emit_health_event("hardware_connected", "startup");
  } else {
    {
      std::lock_guard lock(g_health_mutex);
      g_health.degraded = true;
      g_health.lifecycle = "running";
    }
    logger.log(homepi::logging::LogLevel::WARN, "hifi.serial", "serial_unavailable",
               "startup", "Serial port not available");
    emit_health_event("service_degraded", "startup");
  }

  if (serial_opened) {
    std::thread([&repository, &queue, &logger, log_level]() {
      {
        std::lock_guard lock(g_health_mutex);
        g_health.sync_in_progress = true;
      }
      emit_health_event("sync_started", "startup");
      logger.log(log_level, "hifi.serial", "sync_started", "startup",
                 "Running startup controller sync");
      homepi::hifi_serial::SyncEngine::run_full_sync(queue);
      repository.mark_full_sync_complete();
      {
        std::lock_guard lock(g_health_mutex);
        g_health.sync_in_progress = false;
        g_health.last_full_sync_at = "now";
        g_health.degraded = false;
        g_health.connected = true;
      }
      emit_health_event("sync_completed", "startup");
      emit_health_event("controller_connected", "startup");
      emit_health_event("service_recovered", "startup");
      logger.log(log_level, "hifi.serial", "sync_completed", "startup",
                 "Startup controller sync finished");
    }).detach();
  }

  while (g_running.load()) {
    std::this_thread::sleep_for(std::chrono::seconds(1));
  }

  logger.log(log_level, "core.runtime", "lifecycle_stopping", "shutdown",
             "homepi-hifi-serial stopping");
  server.stop();
  queue.stop();
  port.close();
  return 0;
}
