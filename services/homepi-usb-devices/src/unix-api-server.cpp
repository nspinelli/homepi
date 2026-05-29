#include "homepi/usb-devices/unix-api-server.hpp"

#include <sys/socket.h>
#include <sys/un.h>
#include <unistd.h>

#include <cstdlib>
#include <cstring>
#include <filesystem>
#include <thread>

#include "homepi/usb-devices/artifact-writer.hpp"
#include "homepi/usb-devices/assignment-repository.hpp"
#include "homepi/usb-devices/json-utils.hpp"

namespace fs = std::filesystem;

namespace homepi::usb_devices {

namespace {

constexpr const char* kPostAssignmentHook =
    "/opt/homepi/services/usb-devices/scripts/post-assignment-hook.sh";

/**
 * Deploys udev rules and restarts HiFi serial after assignment changes (non-blocking).
 */
void run_post_assignment_hook_async() {
  std::thread([]() {
    std::system(
        "sudo -n /opt/homepi/services/usb-devices/scripts/post-assignment-hook.sh "
        ">>/opt/homepi/runtime/cache/post-assignment-hook.log 2>&1");
  }).detach();
}

}  // namespace

UnixApiServer::UnixApiServer(ApiContext context) : context_(std::move(context)) {}

UnixApiServer::~UnixApiServer() { stop(); }

bool UnixApiServer::start() {
  if (server_fd_ >= 0) {
    return true;
  }

  std::error_code ec;
  fs::remove(context_.config.socket_path, ec);
  fs::create_directories(context_.config.socket_dir, ec);

  server_fd_ = socket(AF_UNIX, SOCK_STREAM, 0);
  if (server_fd_ < 0) {
    return false;
  }

  sockaddr_un addr{};
  addr.sun_family = AF_UNIX;
  if (context_.config.socket_path.size() >= sizeof(addr.sun_path)) {
    close(server_fd_);
    server_fd_ = -1;
    return false;
  }
  std::strncpy(addr.sun_path, context_.config.socket_path.c_str(), sizeof(addr.sun_path) - 1);

  if (bind(server_fd_, reinterpret_cast<sockaddr*>(&addr), sizeof(addr)) < 0) {
    close(server_fd_);
    server_fd_ = -1;
    return false;
  }

  if (listen(server_fd_, 8) < 0) {
    close(server_fd_);
    server_fd_ = -1;
    return false;
  }

  stop_ = false;
  thread_ = std::thread([this]() { listen_loop(); });
  return true;
}

void UnixApiServer::stop() {
  stop_ = true;
  if (server_fd_ >= 0) {
    shutdown(server_fd_, SHUT_RDWR);
    close(server_fd_);
    server_fd_ = -1;
  }
  if (thread_.joinable()) {
    thread_.join();
  }
  std::error_code ec;
  fs::remove(context_.config.socket_path, ec);
}

void UnixApiServer::listen_loop() {
  while (!stop_.load()) {
    const int client = accept(server_fd_, nullptr, nullptr);
    if (client < 0) {
      if (stop_.load()) {
        break;
      }
      continue;
    }

    std::string buffer;
    char chunk[4096];
    while (true) {
      const ssize_t read_bytes = ::read(client, chunk, sizeof(chunk));
      if (read_bytes <= 0) {
        break;
      }
      buffer.append(chunk, static_cast<std::size_t>(read_bytes));
      if (buffer.find('\n') != std::string::npos) {
        break;
      }
    }

    const auto line_end = buffer.find('\n');
    const std::string line = line_end == std::string::npos ? buffer : buffer.substr(0, line_end);
    const std::string response = handle_request(line) + "\n";
    ::write(client, response.c_str(), response.size());
    close(client);
  }
}

std::string UnixApiServer::handle_request(const std::string& line) const {
  const std::string method = json_get_string(line, "method");
  const std::string correlation_id = json_get_string(line, "correlationId");

  auto ok = [&](const std::string& data_json) {
    std::ostringstream out;
    out << "{\"ok\":true,\"correlationId\":\""
        << (correlation_id.empty() ? "usb-devices" : json_escape(correlation_id))
        << "\",\"data\":" << data_json << "}";
    return out.str();
  };

  auto err = [&](const std::string& message) {
    std::ostringstream out;
    out << "{\"ok\":false,\"correlationId\":\""
        << (correlation_id.empty() ? "usb-devices" : json_escape(correlation_id))
        << "\",\"error\":{\"code\":\"USB_DEVICES_ERROR\",\"message\":\"" << json_escape(message)
        << "\"}}";
    return out.str();
  };

  if (method == "listDevices") {
    if (context_.scan_fn) {
      const auto scanned = context_.scan_fn();
      context_.repository->upsert_devices(scanned);
    }
    return ok(devices_response_json(context_.repository->list_devices()));
  }

  if (method == "getAssignments") {
    return ok(assignments_to_json(context_.repository->get_assignments()));
  }

  if (method == "getHealth") {
    const ServiceHealth health = context_.health_fn ? context_.health_fn() : ServiceHealth{};
    std::ostringstream data;
    data << "{"
         << "\"lifecycle\":\"" << json_escape(health.lifecycle) << "\","
         << "\"udevMonitorActive\":" << (health.udev_monitor_active ? "true" : "false") << ","
         << "\"connectedDeviceCount\":" << health.connected_device_count << ","
         << "\"assignmentsDegraded\":" << (health.assignments_degraded ? "true" : "false")
         << ",\"lastScanAt\":"
         << (health.last_scan_at.empty() ? "null"
                                         : "\"" + json_escape(health.last_scan_at) + "\"")
         << "}";
    return ok(data.str());
  }

  if (method == "setAssignments") {
    UsbAssignments assignments;
    std::string assignments_block = line;
    const auto assign_pos = line.find("\"assignments\"");
    if (assign_pos != std::string::npos) {
      const auto brace = line.find('{', assign_pos);
      const auto end = line.rfind('}');
      if (brace != std::string::npos && end != std::string::npos && end > brace) {
        assignments_block = line.substr(brace, end - brace + 1);
      }
    }
    const std::string serial = json_get_string(assignments_block, "serial");
    const std::string audio_primary = json_get_string(assignments_block, "audioPrimary");
    const std::string paging = json_get_string(assignments_block, "paging");
    if (!serial.empty()) {
      assignments.serial = serial;
    }
    if (!audio_primary.empty()) {
      assignments.audio_primary = audio_primary;
    }
    if (!paging.empty()) {
      assignments.paging = paging;
    }

    if (context_.scan_fn) {
      context_.repository->upsert_devices(context_.scan_fn());
    }
    const auto devices = context_.repository->list_devices();
    std::string error;
    if (!context_.repository->set_assignments(assignments, devices, error)) {
      return err(error);
    }
    if (context_.artifacts != nullptr) {
      context_.artifacts->regenerate(assignments, devices);
    }
    if (fs::exists(kPostAssignmentHook)) {
      run_post_assignment_hook_async();
    }
    if (context_.on_devices_changed) {
      context_.on_devices_changed();
    }
    return ok(assignments_to_json(assignments));
  }

  return err("Unknown method: " + method);
}

}  // namespace homepi::usb_devices
