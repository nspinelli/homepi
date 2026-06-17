#include "homepi/pcm-router/event-subscriber.hpp"

#include <sys/socket.h>
#include <sys/un.h>
#include <unistd.h>

#include <chrono>
#include <cstring>
#include <thread>

#include "homepi/pcm-router/json-utils.hpp"

namespace homepi::pcm_router {

EventSubscriber::~EventSubscriber() { stop(); }

void EventSubscriber::start(const std::string& socket_path, ProfileEventFn on_event) {
  stop();
  socket_path_ = socket_path;
  on_event_ = std::move(on_event);
  stop_ = false;
  thread_ = std::thread([this]() { listen_loop(); });
}

void EventSubscriber::stop() {
  stop_ = true;
  const int fd = connect_fd_.exchange(-1);
  if (fd >= 0) {
    ::shutdown(fd, SHUT_RDWR);
    close(fd);
  }
  if (thread_.joinable()) {
    thread_.join();
  }
}

void EventSubscriber::listen_loop() {
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
        "{\"method\":\"subscribe\",\"correlationId\":\"pcm-router-profile\"}\n";
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
        if (line.empty() || !on_event_) {
          continue;
        }
        const std::string event = parse_event_name(line);
        if (event == "audio_operating_profile_changed" || event == "audio_profile_paused" ||
            event == "audio_profile_invalid" || event == "primary_audio_unassigned") {
          on_event_(event, parse_payload_json(line));
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

}  // namespace homepi::pcm_router
