#include "homepi/metadata/pcm-router-subscriber.hpp"

#include <sys/socket.h>
#include <sys/un.h>
#include <unistd.h>

#include <chrono>
#include <cstring>
#include <thread>

#include "homepi/metadata/json-utils.hpp"

namespace homepi::metadata {

PcmRouterSubscriber::~PcmRouterSubscriber() { stop(); }

void PcmRouterSubscriber::start(const std::string& socket_path, OwnerZoneFn on_owner_change) {
  stop();
  socket_path_ = socket_path;
  on_owner_change_ = std::move(on_owner_change);
  stop_.store(false);
  thread_ = std::thread([this]() { listen_loop(); });
}

void PcmRouterSubscriber::stop() {
  stop_.store(true);
  const int fd = connect_fd_.exchange(-1);
  if (fd >= 0) {
    ::shutdown(fd, SHUT_RDWR);
    close(fd);
  }
  if (thread_.joinable()) {
    thread_.join();
  }
}

int PcmRouterSubscriber::owner_zone_id() const { return owner_zone_id_.load(); }

void PcmRouterSubscriber::listen_loop() {
  while (!stop_.load()) {
    const int fd = socket(AF_UNIX, SOCK_STREAM, 0);
    if (fd < 0) {
      std::this_thread::sleep_for(std::chrono::seconds(5));
      continue;
    }

    sockaddr_un addr{};
    addr.sun_family = AF_UNIX;
    if (socket_path_.size() >= sizeof(addr.sun_path)) {
      close(fd);
      std::this_thread::sleep_for(std::chrono::seconds(5));
      continue;
    }
    std::strncpy(addr.sun_path, socket_path_.c_str(), sizeof(addr.sun_path) - 1);

    if (connect(fd, reinterpret_cast<sockaddr*>(&addr), sizeof(addr)) < 0) {
      close(fd);
      std::this_thread::sleep_for(std::chrono::seconds(5));
      continue;
    }

    connect_fd_.store(fd);
    const std::string subscribe =
        "{\"method\":\"subscribe\",\"correlationId\":\"homepi-metadata\"}\n";
    if (write(fd, subscribe.c_str(), subscribe.size()) < 0) {
      close(fd);
      std::this_thread::sleep_for(std::chrono::seconds(5));
      continue;
    }

    std::string buffer;
    char chunk[4096];
    while (!stop_.load()) {
      const ssize_t n = read(fd, chunk, sizeof(chunk));
      if (n <= 0) {
        break;
      }
      buffer.append(chunk, static_cast<std::size_t>(n));
      std::size_t pos = 0;
      while ((pos = buffer.find('\n')) != std::string::npos) {
        const std::string line = buffer.substr(0, pos);
        buffer.erase(0, pos + 1);
        if (!line.empty()) {
          handle_line(line);
        }
      }
    }

    close(fd);
    connect_fd_.store(-1);
    if (!stop_.load()) {
      std::this_thread::sleep_for(std::chrono::seconds(5));
    }
  }
}

void PcmRouterSubscriber::handle_line(const std::string& line) {
  const std::string event = parse_event_name(line);
  if (event != "pcm_router_snapshot" && event != "routing_changed") {
    return;
  }
  const std::string payload = parse_payload_json(line);
  const int owner = parse_int_field(payload, "ownerZoneId");
  const int previous = owner_zone_id_.exchange(owner);
  if (previous != owner && on_owner_change_) {
    on_owner_change_(owner);
  }
}

}  // namespace homepi::metadata
