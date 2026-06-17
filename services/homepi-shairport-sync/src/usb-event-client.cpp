#include "homepi/shairport-sync/usb-event-client.hpp"

#include <sys/socket.h>
#include <sys/un.h>
#include <unistd.h>

#include <chrono>
#include <cstring>
#include <thread>

namespace homepi::shairport_sync {

UsbEventClient::~UsbEventClient() { stop(); }

void UsbEventClient::start(const std::string& socket_path,
                           std::function<void(const std::string&)> on_event) {
  stop();
  socket_path_ = socket_path;
  on_event_ = std::move(on_event);
  stop_ = false;
  thread_ = std::thread([this]() { listen_loop(); });
}

void UsbEventClient::stop() {
  stop_ = true;
  if (thread_.joinable()) {
    thread_.join();
  }
}

void UsbEventClient::listen_loop() {
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
        "{\"method\":\"subscribe\",\"correlationId\":\"shairport-profile\"}\n";
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
