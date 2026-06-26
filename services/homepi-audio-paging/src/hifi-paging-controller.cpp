#include "homepi/audio-paging/hifi-paging-controller.hpp"

#include <chrono>
#include <sys/socket.h>
#include <sys/un.h>
#include <thread>
#include <unistd.h>

#include <cstring>

#include "homepi/audio-paging/json-utils.hpp"
#include "homepi/events/event-emitter.hpp"

namespace homepi::audio_paging {

namespace {

int connect_unix_socket(const std::string& path) {
  const int fd = socket(AF_UNIX, SOCK_STREAM, 0);
  if (fd < 0) {
    return -1;
  }
  sockaddr_un address{};
  address.sun_family = AF_UNIX;
  if (path.size() >= sizeof(address.sun_path)) {
    close(fd);
    return -1;
  }
  std::strncpy(address.sun_path, path.c_str(), sizeof(address.sun_path) - 1);
  if (connect(fd, reinterpret_cast<sockaddr*>(&address), sizeof(address)) < 0) {
    close(fd);
    return -1;
  }
  return fd;
}

}  // namespace

HifiPagingController::HifiPagingController(homepi::events::EventEmitter* emitter,
                                           std::string hifi_serial_socket)
    : emitter_(emitter), hifi_serial_socket_(std::move(hifi_serial_socket)) {}

void HifiPagingController::request_page_on(const std::string& correlation_id) {
  sync_page_state_from_hifi();
  if (emitter_ == nullptr) {
    return;
  }
  emitter_->emit("modules.hifi.command", "page_start", correlation_id, "{}");
}

void HifiPagingController::request_page_off(const std::string& correlation_id) {
  if (emitter_ == nullptr) {
    return;
  }
  emitter_->emit("modules.hifi.command", "page_end", correlation_id, "{}");
}

void HifiPagingController::on_page_state_changed(int page_state) { page_state_ = page_state; }

void HifiPagingController::sync_page_state_from_hifi() {
  if (hifi_serial_socket_.empty()) {
    return;
  }
  const int fd = connect_unix_socket(hifi_serial_socket_);
  if (fd < 0) {
    return;
  }
  const std::string request =
      "{\"method\":\"getSnapshot\",\"correlationId\":\"audio-paging-page-sync\"}\n";
  if (write(fd, request.c_str(), request.size()) < 0) {
    close(fd);
    return;
  }
  char buffer[16384];
  const ssize_t bytes = read(fd, buffer, sizeof(buffer) - 1);
  close(fd);
  if (bytes <= 0) {
    return;
  }
  buffer[bytes] = '\0';
  const std::string line(buffer);
  const std::string data = json_get_object(line, "data");
  const std::string controller = json_get_object(data, "controller");
  const std::string page_active = json_get_scalar(controller, "pageActive");
  if (!page_active.empty()) {
    page_state_ = std::stoi(page_active);
  }
}

bool HifiPagingController::wait_for_page_on(int timeout_ms) {
  const auto deadline = std::chrono::steady_clock::now() + std::chrono::milliseconds(timeout_ms);
  while (std::chrono::steady_clock::now() < deadline) {
    sync_page_state_from_hifi();
    if (page_state_.load() == 1) {
      return true;
    }
    if (page_state_.load() == 3) {
      return false;
    }
    std::this_thread::sleep_for(std::chrono::milliseconds(50));
  }
  return false;
}

bool HifiPagingController::wait_for_page_off(int timeout_ms) {
  const auto deadline = std::chrono::steady_clock::now() + std::chrono::milliseconds(timeout_ms);
  while (std::chrono::steady_clock::now() < deadline) {
    sync_page_state_from_hifi();
    if (page_state_.load() == 0) {
      return true;
    }
    std::this_thread::sleep_for(std::chrono::milliseconds(50));
  }
  return false;
}

bool HifiPagingController::any_zone_powered_in_snapshot() {
  if (hifi_serial_socket_.empty()) {
    return false;
  }
  const int fd = connect_unix_socket(hifi_serial_socket_);
  if (fd < 0) {
    return false;
  }
  const std::string request =
      "{\"method\":\"getSnapshot\",\"correlationId\":\"audio-paging-zone-sync\"}\n";
  if (write(fd, request.c_str(), request.size()) < 0) {
    close(fd);
    return false;
  }
  char buffer[16384];
  const ssize_t bytes = read(fd, buffer, sizeof(buffer) - 1);
  close(fd);
  if (bytes <= 0) {
    return false;
  }
  buffer[bytes] = '\0';
  const std::string line(buffer);
  const std::string data = json_get_object(line, "data");
  const std::string zones = json_get_object(data, "zones");
  std::size_t pos = 0;
  while (pos < zones.size()) {
    const std::size_t power_key = zones.find("\"power\"", pos);
    if (power_key == std::string::npos) {
      break;
    }
    const std::string power_value = json_get_scalar(zones.substr(power_key), "power");
    if (power_value == "1") {
      return true;
    }
    pos = power_key + 7;
  }
  return false;
}

bool HifiPagingController::wait_for_page_playback_ready(int page_on_timeout_ms, int settle_ms) {
  if (!wait_for_page_on(page_on_timeout_ms)) {
    return false;
  }
  const auto deadline =
      std::chrono::steady_clock::now() + std::chrono::milliseconds(page_on_timeout_ms);
  while (std::chrono::steady_clock::now() < deadline) {
    if (any_zone_powered_in_snapshot()) {
      break;
    }
    std::this_thread::sleep_for(std::chrono::milliseconds(50));
  }
  if (settle_ms > 0) {
    std::this_thread::sleep_for(std::chrono::milliseconds(settle_ms));
  }
  return true;
}

bool HifiPagingController::external_page_active() const { return page_state_.load() == 3; }

}  // namespace homepi::audio_paging
