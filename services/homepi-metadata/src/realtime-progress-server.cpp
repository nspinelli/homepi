#include "homepi/metadata/realtime-progress-server.hpp"

#include <sys/epoll.h>
#include <sys/socket.h>
#include <sys/un.h>
#include <unistd.h>

#include <array>
#include <chrono>
#include <cerrno>
#include <cstring>
#include <filesystem>
#include <sstream>
#include <unordered_map>

#include "homepi/events/event-envelope.hpp"
#include "homepi/metadata/json-utils.hpp"
#include "homepi/transport/latest-value-publisher.hpp"
#include "homepi/transport/unix-socket.hpp"

namespace fs = std::filesystem;

namespace homepi::metadata {

namespace {

constexpr int kEpollMaxEvents = 32;
constexpr int kClientBufferMax = 4096;

}  // namespace

RealtimeProgressServer::RealtimeProgressServer()
    : publisher_(std::make_unique<homepi::transport::LatestValuePublisher>()) {}

RealtimeProgressServer::~RealtimeProgressServer() { stop(); }

bool RealtimeProgressServer::start(const std::string& socket_path) {
  if (server_fd_ >= 0) {
    return true;
  }

  socket_path_ = socket_path;
  homepi::transport::remove_socket_path(socket_path_);
  std::error_code ec;
  fs::create_directories(fs::path(socket_path_).parent_path(), ec);

  server_fd_ = homepi::transport::create_listening_unix_stream_socket(socket_path_, 16);
  if (server_fd_ < 0) {
    return false;
  }
  homepi::transport::apply_socket_permissions(socket_path_, 0660, "homepi");
  homepi::transport::set_nonblocking(server_fd_);

  epoll_fd_ = epoll_create1(0);
  if (epoll_fd_ < 0) {
    stop();
    return false;
  }

  epoll_event server_event{};
  server_event.events = EPOLLIN;
  server_event.data.fd = server_fd_;
  if (epoll_ctl(epoll_fd_, EPOLL_CTL_ADD, server_fd_, &server_event) < 0) {
    stop();
    return false;
  }

  stop_.store(false);
  thread_ = std::thread([this]() { listen_loop(); });
  return true;
}

void RealtimeProgressServer::stop() {
  stop_.store(true);
  if (server_fd_ >= 0) {
    shutdown(server_fd_, SHUT_RDWR);
  }
  if (thread_.joinable()) {
    thread_.join();
  }
  if (epoll_fd_ >= 0) {
    close(epoll_fd_);
    epoll_fd_ = -1;
  }
  if (server_fd_ >= 0) {
    close(server_fd_);
    server_fd_ = -1;
  }
  if (!socket_path_.empty()) {
    homepi::transport::remove_socket_path(socket_path_);
  }
}

void RealtimeProgressServer::publish_progress(int owner_zone_id, const std::string& track_id,
                                              bool playing, int position_ms, int duration_ms,
                                              const std::string& progress_source) {
  {
    std::lock_guard lock(state_mutex_);
    owner_zone_id_ = owner_zone_id;
    track_id_ = track_id;
    playing_ = playing;
    position_ms_ = position_ms;
    duration_ms_ = duration_ms;
    progress_source_ = progress_source;
  }

  const std::string frame =
      build_frame(owner_zone_id, track_id, playing, position_ms, duration_ms, progress_source) +
      "\n";
  publisher_->publish(frame);

  for (const int fd : publisher_->clients_with_pending()) {
    epoll_event event{};
    event.events = EPOLLIN | EPOLLOUT | EPOLLET;
    event.data.fd = fd;
    epoll_ctl(epoll_fd_, EPOLL_CTL_MOD, fd, &event);
  }
}

std::string RealtimeProgressServer::build_frame(int owner_zone_id, const std::string& track_id,
                                                bool playing, int position_ms, int duration_ms,
                                                const std::string& progress_source) {
  const auto now = std::chrono::system_clock::now();
  const auto ms = std::chrono::duration_cast<std::chrono::milliseconds>(now.time_since_epoch())
                      .count();
  const std::uint64_t sequence = sequence_.fetch_add(1) + 1;

  std::ostringstream out;
  out << "{"
      << "\"type\":\"audio.realtime.snapshot\","
      << "\"schemaVersion\":1,"
      << "\"sequence\":" << sequence << ","
      << "\"monotonicMs\":" << ms << ","
      << "\"wallTime\":\"" << homepi::events::iso_timestamp() << "\","
      << "\"payload\":{"
      << "\"ownerZoneId\":" << owner_zone_id << ","
      << "\"trackId\":\"" << escape_json_string(track_id) << "\","
      << "\"playing\":" << (playing ? "true" : "false") << ","
      << "\"positionMs\":" << position_ms << ","
      << "\"durationMs\":" << duration_ms << ","
      << "\"progressSource\":\"" << escape_json_string(progress_source) << "\""
      << "}}";
  return out.str();
}

void RealtimeProgressServer::listen_loop() {
  std::array<epoll_event, kEpollMaxEvents> events{};
  std::unordered_map<int, std::string> client_buffers;

  while (!stop_.load()) {
    const int ready = epoll_wait(epoll_fd_, events.data(), kEpollMaxEvents, 250);
    if (ready < 0) {
      if (errno == EINTR) {
        continue;
      }
      break;
    }

    for (int i = 0; i < ready; ++i) {
      const int fd = events[i].data.fd;

      if (fd == server_fd_) {
        while (true) {
          const int client_fd = accept(server_fd_, nullptr, nullptr);
          if (client_fd < 0) {
            break;
          }
          homepi::transport::set_nonblocking(client_fd);
          publisher_->add_client(client_fd);
          epoll_event client_event{};
          client_event.events = EPOLLIN | EPOLLOUT | EPOLLET;
          client_event.data.fd = client_fd;
          epoll_ctl(epoll_fd_, EPOLL_CTL_ADD, client_fd, &client_event);

          int owner = 0;
          std::string track_id;
          bool playing = false;
          int position = 0;
          int duration = 0;
          std::string source;
          {
            std::lock_guard lock(state_mutex_);
            owner = owner_zone_id_;
            track_id = track_id_;
            playing = playing_;
            position = position_ms_;
            duration = duration_ms_;
            source = progress_source_;
          }
          const std::string frame =
              build_frame(owner, track_id, playing, position, duration, source) + "\n";
          publisher_->publish(frame);
        }
        continue;
      }

      if (events[i].events & (EPOLLIN | EPOLLRDHUP)) {
        char chunk[512];
        while (true) {
          const ssize_t n = read(fd, chunk, sizeof(chunk));
          if (n < 0) {
            if (errno == EAGAIN || errno == EWOULDBLOCK) {
              break;
            }
            publisher_->remove_client(fd);
            client_buffers.erase(fd);
            close(fd);
            break;
          }
          if (n == 0) {
            publisher_->remove_client(fd);
            client_buffers.erase(fd);
            close(fd);
            break;
          }
          client_buffers[fd].append(chunk, static_cast<std::size_t>(n));
          if (client_buffers[fd].size() > kClientBufferMax) {
            client_buffers[fd].clear();
          }
          std::size_t pos = 0;
          while ((pos = client_buffers[fd].find('\n')) != std::string::npos) {
            const std::string line = client_buffers[fd].substr(0, pos);
            client_buffers[fd].erase(0, pos + 1);
            if (!line.empty()) {
              handle_client_line(fd, line);
            }
          }
        }
      }

      if (events[i].events & EPOLLOUT) {
        if (publisher_->handle_writable(fd)) {
          epoll_event client_event{};
          client_event.events = EPOLLIN | EPOLLET;
          client_event.data.fd = fd;
          epoll_ctl(epoll_fd_, EPOLL_CTL_MOD, fd, &client_event);
        }
      }
    }
  }

  for (const auto& [fd, _] : client_buffers) {
    publisher_->remove_client(fd);
    close(fd);
  }
}

void RealtimeProgressServer::handle_client_line(int client_fd, const std::string& line) {
  (void)client_fd;
  if (line.find("subscribeRealtime") == std::string::npos &&
      line.find("\"method\":\"subscribe\"") == std::string::npos) {
    return;
  }

  int owner = 0;
  std::string track_id;
  bool playing = false;
  int position = 0;
  int duration = 0;
  std::string source;
  {
    std::lock_guard lock(state_mutex_);
    owner = owner_zone_id_;
    track_id = track_id_;
    playing = playing_;
    position = position_ms_;
    duration = duration_ms_;
    source = progress_source_;
  }
  const std::string frame =
      build_frame(owner, track_id, playing, position, duration, source) + "\n";
  publisher_->publish(frame);
}

}  // namespace homepi::metadata
