#include "homepi/usb-devices/unix-api-server.hpp"

#include <sys/socket.h>
#include <sys/un.h>
#include <unistd.h>

#include <chrono>
#include <cstdlib>
#include <cstring>
#include <filesystem>
#include <iomanip>
#include <sstream>
#include <thread>

#include "homepi/usb-devices/artifact-writer.hpp"
#include "homepi/usb-devices/assignment-repository.hpp"
#include "homepi/usb-devices/audio-profile-service.hpp"
#include "homepi/usb-devices/json-utils.hpp"
#include "homepi/storage/audio-profile-types.hpp"

namespace fs = std::filesystem;

namespace homepi::usb_devices {

namespace {

constexpr const char* kPostAssignmentHook =
    "/opt/homepi/services/usb-devices/scripts/post-assignment-hook.sh";

std::optional<homepi::storage::AudioProfileTuple> parse_audio_primary_profile(
    const std::string& assignments_block) {
  const auto profile_pos = assignments_block.find("\"audioPrimaryProfile\"");
  if (profile_pos == std::string::npos) {
    return std::nullopt;
  }
  const auto brace = assignments_block.find('{', profile_pos);
  const auto end = assignments_block.find('}', brace);
  if (brace == std::string::npos || end == std::string::npos) {
    return std::nullopt;
  }
  const std::string profile_object = assignments_block.substr(brace, end - brace + 1);
  const std::string sample_rate = json_get_scalar(profile_object, "sampleRate");
  const std::string channels = json_get_scalar(profile_object, "channels");
  const std::string sample_format = json_get_string(profile_object, "sampleFormat");
  if (sample_rate.empty() || channels.empty() || sample_format.empty()) {
    return std::nullopt;
  }
  homepi::storage::AudioProfileTuple tuple;
  tuple.sample_rate = static_cast<uint32_t>(std::stoul(sample_rate));
  tuple.channels = static_cast<uint16_t>(std::stoul(channels));
  tuple.sample_format =
      homepi::storage::parse_sample_format(sample_format).value_or(homepi::storage::SampleFormat::S16Le);
  return tuple;
}

bool profiles_equal(const std::optional<homepi::storage::AudioProfileTuple>& left,
                    const std::optional<homepi::storage::AudioProfileTuple>& right) {
  if (!left.has_value() && !right.has_value()) {
    return true;
  }
  if (!left.has_value() || !right.has_value()) {
    return false;
  }
  return left->sample_rate == right->sample_rate && left->channels == right->channels &&
         left->sample_format == right->sample_format;
}

void run_post_assignment_hook(bool serial_changed, bool audio_changed) {
  if (!serial_changed && !audio_changed) {
    return;
  }
  std::ostringstream command;
  command << "sudo -n " << kPostAssignmentHook << ' ' << (serial_changed ? "1" : "0") << ' '
          << (audio_changed ? "1" : "0")
          << " >>/opt/homepi/runtime/cache/post-assignment-hook.log 2>&1";
  std::system(command.str().c_str());
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

  if (listen(server_fd_, 16) < 0) {
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
  std::lock_guard lock(clients_mutex_);
  for (int fd : subscribers_) {
    close(fd);
  }
  subscribers_.clear();
  std::error_code ec;
  fs::remove(context_.config.socket_path, ec);
}

void UnixApiServer::broadcast(const std::string& line) {
  const std::string frame = line + "\n";
  std::lock_guard lock(clients_mutex_);
  for (auto it = subscribers_.begin(); it != subscribers_.end();) {
    const ssize_t written = ::write(*it, frame.c_str(), frame.size());
    if (written < 0) {
      close(*it);
      it = subscribers_.erase(it);
    } else {
      ++it;
    }
  }
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
    std::thread([this, client]() { handle_client(client); }).detach();
  }
}

void UnixApiServer::handle_client(int client_fd) {
  bool subscribed = false;
  std::string buffer;
  char chunk[4096];

  while (!stop_.load()) {
    const ssize_t n = ::read(client_fd, chunk, sizeof(chunk));
    if (n <= 0) {
      break;
    }
    buffer.append(chunk, static_cast<std::size_t>(n));

    std::size_t pos = 0;
    while ((pos = buffer.find('\n')) != std::string::npos) {
      const std::string line = buffer.substr(0, pos);
      buffer.erase(0, pos + 1);
      if (line.empty()) {
        continue;
      }

      const std::string method = json_get_string(line, "method");
      if (method == "subscribe") {
        if (!subscribed) {
          subscribed = true;
          {
            std::lock_guard lock(clients_mutex_);
            subscribers_.insert(client_fd);
          }
          if (context_.on_subscribe_fn) {
            context_.on_subscribe_fn();
          }
        }
        const std::string correlation_id = json_get_string(line, "correlationId");
        const std::string response =
            "{\"ok\":true,\"correlationId\":\"" +
            json_escape(correlation_id.empty() ? "usb-devices" : correlation_id) +
            "\",\"data\":{\"subscribed\":true}}\n";
        ::write(client_fd, response.c_str(), response.size());
        continue;
      }

      const std::string response = handle_request(line) + "\n";
      ::write(client_fd, response.c_str(), response.size());
      if (method != "subscribe") {
        break;
      }
    }
  }

  {
    std::lock_guard lock(clients_mutex_);
    subscribers_.erase(client_fd);
  }
  close(client_fd);
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
    auto assignments = context_.repository->get_assignments();
    const auto devices = context_.repository->list_devices();
    if (AssignmentRepository::heal_assignments(assignments, devices)) {
      context_.repository->persist_healed_assignments(assignments, devices);
    }
    return ok(assignments_to_json(assignments));
  }

  if (method == "getAudioCapabilities") {
    const std::string device_id = json_get_string(line, "deviceId");
    if (device_id.empty() || context_.audio_profiles == nullptr) {
      return err("deviceId is required");
    }
    return ok(context_.audio_profiles->capabilities_json(device_id));
  }

  if (method == "getOperatingProfile") {
    if (context_.audio_profiles == nullptr) {
      return err("audio profile service unavailable");
    }
    return ok(context_.audio_profiles->operating_profile_json());
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
    if (const auto profile = parse_audio_primary_profile(assignments_block)) {
      assignments.audio_primary_profile = *profile;
    }

    if (context_.scan_fn) {
      context_.repository->upsert_devices(context_.scan_fn());
    }
    const auto previous = context_.repository->get_assignments();
    const auto devices = context_.repository->list_devices();
    if (context_.audio_profiles != nullptr) {
      context_.audio_profiles->refresh_audio_capabilities(devices);
    }
    std::string error;
    if (context_.audio_profiles == nullptr) {
      if (!context_.repository->set_assignments(assignments, devices, error)) {
        return err(error);
      }
    } else if (!context_.audio_profiles->apply_assignments(assignments, devices, correlation_id,
                                                           error)) {
      return err(error);
    }
    if (context_.artifacts != nullptr) {
      context_.artifacts->regenerate(assignments, devices);
    }
    const bool serial_changed = assignments.serial != previous.serial;
    const bool audio_changed =
        assignments.audio_primary != previous.audio_primary ||
        assignments.paging != previous.paging ||
        !profiles_equal(assignments.audio_primary_profile, previous.audio_primary_profile);
    if (fs::exists(kPostAssignmentHook)) {
      run_post_assignment_hook(serial_changed, audio_changed);
    }
    return ok(assignments_to_json(context_.repository->get_assignments()));
  }

  return err("Unknown method: " + method);
}

}  // namespace homepi::usb_devices
