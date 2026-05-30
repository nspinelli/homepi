#include "homepi/shairport-sync/hifi-event-client.hpp"

#include <sys/socket.h>
#include <sys/un.h>
#include <unistd.h>

#include <chrono>
#include <cstring>
#include <thread>

namespace homepi::shairport_sync {

HifiEventClient::~HifiEventClient() { stop(); }

void HifiEventClient::start(const std::string& socket_path,
                            std::function<void(const std::string&)> on_event) {
  stop();
  socket_path_ = socket_path;
  on_event_ = std::move(on_event);
  stop_ = false;
  thread_ = std::thread([this]() { listen_loop(); });
}

void HifiEventClient::stop() {
  stop_ = true;
  if (thread_.joinable()) {
    thread_.join();
  }
}

std::string HifiEventClient::rpc(const std::string& socket_path, const std::string& request) {
  const int fd = socket(AF_UNIX, SOCK_STREAM, 0);
  if (fd < 0) {
    return {};
  }

  sockaddr_un addr{};
  addr.sun_family = AF_UNIX;
  if (socket_path.size() >= sizeof(addr.sun_path)) {
    close(fd);
    return {};
  }
  std::strncpy(addr.sun_path, socket_path.c_str(), sizeof(addr.sun_path) - 1);

  if (connect(fd, reinterpret_cast<sockaddr*>(&addr), sizeof(addr)) < 0) {
    close(fd);
    return {};
  }

  const std::string frame = request + "\n";
  if (write(fd, frame.c_str(), frame.size()) < 0) {
    close(fd);
    return {};
  }

  char buffer[8192];
  const ssize_t n = read(fd, buffer, sizeof(buffer) - 1);
  close(fd);
  if (n <= 0) {
    return {};
  }
  buffer[n] = '\0';
  return std::string(buffer);
}

void HifiEventClient::listen_loop() {
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

    const std::string subscribe =
        "{\"method\":\"subscribe\",\"correlationId\":\"shairport-supervisor\"}\n";
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
        if (!line.empty() && on_event_) {
          on_event_(line);
        }
      }
    }

    close(fd);
    if (!stop_.load()) {
      std::this_thread::sleep_for(std::chrono::seconds(5));
    }
  }
}

}  // namespace homepi::shairport_sync
