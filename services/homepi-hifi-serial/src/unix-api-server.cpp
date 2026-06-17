#include "homepi/hifi-serial/unix-api-server.hpp"

#include <sys/socket.h>
#include <sys/un.h>
#include <unistd.h>

#include <cerrno>
#include <chrono>
#include <cstring>
#include <filesystem>
#include <iostream>
#include <iomanip>
#include <sstream>

#include "homepi/hifi-serial/json-utils.hpp"

namespace fs = std::filesystem;

namespace homepi::hifi_serial {

namespace {

std::string iso_timestamp() {
  const auto now = std::chrono::system_clock::now();
  const std::time_t t = std::chrono::system_clock::to_time_t(now);
  std::tm tm{};
  gmtime_r(&t, &tm);
  std::ostringstream out;
  out << std::put_time(&tm, "%Y-%m-%dT%H:%M:%S") << ".000Z";
  return out.str();
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
    const int bind_errno = errno;
    std::cerr << "hifi-serial socket bind failed path=" << context_.config.socket_path
              << " errno=" << bind_errno << " " << std::strerror(bind_errno) << "\n";
    close(server_fd_);
    server_fd_ = -1;
    errno = bind_errno;
    return false;
  }

  if (listen(server_fd_, 16) < 0) {
    const int listen_errno = errno;
    std::cerr << "hifi-serial socket listen failed path=" << context_.config.socket_path
              << " errno=" << listen_errno << " " << std::strerror(listen_errno) << "\n";
    close(server_fd_);
    server_fd_ = -1;
    errno = listen_errno;
    return false;
  }

  stop_ = false;
  thread_ = std::thread([this]() { listen_loop(); });
  return true;
}

void UnixApiServer::stop() {
  stop_ = true;
  if (server_fd_ >= 0) {
    ::shutdown(server_fd_, SHUT_RDWR);
    close(server_fd_);
    server_fd_ = -1;
  }
  {
    std::lock_guard lock(clients_mutex_);
    for (int fd : subscribers_) {
      ::shutdown(fd, SHUT_RDWR);
      close(fd);
    }
    subscribers_.clear();
    for (int fd : active_clients_) {
      ::shutdown(fd, SHUT_RDWR);
      close(fd);
    }
    active_clients_.clear();
  }
  if (thread_.joinable()) {
    thread_.join();
  }
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
  {
    std::lock_guard lock(clients_mutex_);
    active_clients_.insert(client_fd);
  }

  bool subscribed = false;
  std::string buffer;
  char chunk[4096];

  auto send_snapshot = [&]() {
    if (!context_.snapshot_json_fn) {
      return;
    }
    const std::string payload = context_.snapshot_json_fn();
    std::ostringstream snap;
    snap << "{"
         << "\"version\":1,"
         << "\"id\":\"evt-snapshot\","
         << "\"source\":\"homepi-hifi-serial\","
         << "\"topic\":\"modules.audio.snapshot\","
         << "\"event\":\"audio_state_snapshot\","
         << "\"correlationId\":\"connect\","
         << "\"timestamp\":\"" << iso_timestamp() << "\","
         << "\"payload\":" << payload
         << "}";
    const std::string frame = snap.str() + "\n";
    ::write(client_fd, frame.c_str(), frame.size());
  };

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
      if (!method.empty() && context_.handle_rpc_fn) {
        const std::string response = context_.handle_rpc_fn(line) + "\n";
        ::write(client_fd, response.c_str(), response.size());
        if (method == "subscribe") {
          if (!subscribed) {
            subscribed = true;
            {
              std::lock_guard lock(clients_mutex_);
              subscribers_.insert(client_fd);
            }
            send_snapshot();
            if (context_.on_subscribe_fn) {
              context_.on_subscribe_fn();
            }
          }
          continue;
        }
        break;
      }

      if (!subscribed) {
        subscribed = true;
        {
          std::lock_guard lock(clients_mutex_);
          subscribers_.insert(client_fd);
        }
        send_snapshot();
      }
    }
  }

  {
    std::lock_guard lock(clients_mutex_);
    subscribers_.erase(client_fd);
    active_clients_.erase(client_fd);
  }
  close(client_fd);
}

}  // namespace homepi::hifi_serial
