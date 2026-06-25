#include "homepi/metadata/realtime-progress-server.hpp"

#include <sys/epoll.h>
#include <sys/socket.h>
#include <sys/un.h>
#include <unistd.h>

#include <algorithm>
#include <chrono>
#include <cerrno>
#include <cstring>
#include <filesystem>
#include <sstream>

#include "homepi/events/event-envelope.hpp"
#include "homepi/metadata/json-utils.hpp"
#include "homepi/metadata/service-event-loop.hpp"
#include "homepi/transport/latest-value-publisher.hpp"
#include "homepi/transport/unix-socket.hpp"

namespace fs = std::filesystem;

namespace homepi::metadata {

namespace {

constexpr int kClientBufferMax = 4096;

}  // namespace

RealtimeProgressServer::RealtimeProgressServer()
    : publisher_(std::make_unique<homepi::transport::LatestValuePublisher>()) {}

RealtimeProgressServer::~RealtimeProgressServer() { stop(); }

bool RealtimeProgressServer::prepare(const std::string& socket_path) {
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
  stop_.store(false);
  return true;
}

void RealtimeProgressServer::attach(ServiceEventLoop& loop) {
  loop_ = &loop;
  loop.add_readable(server_fd_, [this](int fd) {
    if (fd == server_fd_) {
      accept_clients();
    }
  });
}

void RealtimeProgressServer::stop() {
  stop_.store(true);
  if (loop_ != nullptr && server_fd_ >= 0) {
    loop_->remove_fd(server_fd_);
  }
  for (const auto& [fd, _] : client_buffers_) {
    if (loop_ != nullptr) {
      loop_->remove_fd(fd);
    }
    publisher_->remove_client(fd);
    close(fd);
  }
  client_buffers_.clear();
  if (server_fd_ >= 0) {
    shutdown(server_fd_, SHUT_RDWR);
    close(server_fd_);
    server_fd_ = -1;
  }
  if (!socket_path_.empty()) {
    homepi::transport::remove_socket_path(socket_path_);
  }
  loop_ = nullptr;
}

void RealtimeProgressServer::accept_clients() {
  while (!stop_.load()) {
    const int client_fd = accept(server_fd_, nullptr, nullptr);
    if (client_fd < 0) {
      break;
    }
    homepi::transport::set_nonblocking(client_fd);
    publisher_->add_client(client_fd);
    client_buffers_[client_fd] = {};
    if (loop_ != nullptr) {
      loop_->add_io_handler(client_fd, EPOLLIN | EPOLLOUT | EPOLLET,
                            [this](int fd, uint32_t events) {
                              if (events & (EPOLLIN | EPOLLRDHUP)) {
                                handle_client_readable(fd);
                              }
                              if (events & EPOLLOUT) {
                                handle_client_writable(fd);
                              }
                            });
    }
    send_initial_frame(client_fd);
  }
}

void RealtimeProgressServer::handle_client_readable(int client_fd) {
  char chunk[512];
  while (true) {
    const ssize_t n = read(client_fd, chunk, sizeof(chunk));
    if (n < 0) {
      if (errno == EAGAIN || errno == EWOULDBLOCK) {
        break;
      }
      remove_client(client_fd);
      break;
    }
    if (n == 0) {
      remove_client(client_fd);
      break;
    }
    client_buffers_[client_fd].append(chunk, static_cast<std::size_t>(n));
    if (client_buffers_[client_fd].size() > kClientBufferMax) {
      client_buffers_[client_fd].clear();
    }
    std::size_t pos = 0;
    while ((pos = client_buffers_[client_fd].find('\n')) != std::string::npos) {
      const std::string line = client_buffers_[client_fd].substr(0, pos);
      client_buffers_[client_fd].erase(0, pos + 1);
      if (!line.empty()) {
        handle_client_line(client_fd, line);
      }
    }
  }
}

void RealtimeProgressServer::handle_client_writable(int client_fd) {
  if (publisher_->handle_writable(client_fd) && loop_ != nullptr) {
    loop_->mod_io_events(client_fd, EPOLLIN | EPOLLET);
  }
}

void RealtimeProgressServer::remove_client(int client_fd) {
  if (loop_ != nullptr) {
    loop_->remove_fd(client_fd);
  }
  publisher_->remove_client(client_fd);
  client_buffers_.erase(client_fd);
  close(client_fd);
}

void RealtimeProgressServer::send_initial_frame(int client_fd) {
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
  const std::string frame = build_frame(owner, track_id, playing, position, duration, source) + "\n";
  publisher_->publish(frame);
  if (loop_ != nullptr) {
    loop_->mod_io_events(client_fd, EPOLLIN | EPOLLOUT | EPOLLET);
  }
  (void)client_fd;
}

void RealtimeProgressServer::publish_progress(int owner_zone_id, const std::string& track_id,
                                              bool playing, int position_ms, int duration_ms,
                                              const std::string& progress_source) {
  bool timing_changed = false;
  {
    std::lock_guard lock(state_mutex_);
    timing_changed = position_ms != position_ms_ || duration_ms != duration_ms_ ||
                     playing != playing_ || owner_zone_id != owner_zone_id_ ||
                     track_id != track_id_;
    owner_zone_id_ = owner_zone_id;
    track_id_ = track_id;
    playing_ = playing;
    position_ms_ = position_ms;
    duration_ms_ = duration_ms;
    progress_source_ = progress_source;
  }

  const auto now = std::chrono::steady_clock::now();
  const int max_hz = std::max(1, publish_max_hz_);
  const auto min_interval = std::chrono::milliseconds(1000 / max_hz);
  if (!timing_changed && last_publish_at_.time_since_epoch().count() != 0 &&
      now - last_publish_at_ < min_interval) {
    return;
  }
  last_publish_at_ = now;

  const std::string frame =
      build_frame(owner_zone_id, track_id, playing, position_ms, duration_ms, progress_source) +
      "\n";
  publisher_->publish(frame);

  if (loop_ == nullptr) {
    return;
  }
  for (const int fd : publisher_->clients_with_pending()) {
    loop_->mod_io_events(fd, EPOLLIN | EPOLLOUT | EPOLLET);
  }
}

std::string RealtimeProgressServer::build_frame(int owner_zone_id, const std::string& track_id,
                                                bool playing, int position_ms, int duration_ms,
                                                const std::string& progress_source) {
  const auto now = std::chrono::steady_clock::now();
  const auto monotonic_ms =
      std::chrono::duration_cast<std::chrono::milliseconds>(now - started_at_).count();
  const std::uint64_t sequence = sequence_.fetch_add(1) + 1;

  std::ostringstream out;
  out << "{"
      << "\"type\":\"audio.realtime.snapshot\","
      << "\"schemaVersion\":1,"
      << "\"sequence\":" << sequence << ","
      << "\"monotonicMs\":" << monotonic_ms << ","
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

void RealtimeProgressServer::handle_client_line(int client_fd, const std::string& line) {
  (void)client_fd;
  if (line.find("subscribeRealtime") == std::string::npos &&
      line.find("\"method\":\"subscribe\"") == std::string::npos) {
    return;
  }

  const int parsed_hz = parse_int_field(line, "maxHz");
  if (parsed_hz > 0) {
    publish_max_hz_ = std::min(parsed_hz, 10);
  }

  last_publish_at_ = {};
  send_initial_frame(client_fd);
}

}  // namespace homepi::metadata
