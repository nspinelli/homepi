#include "homepi/pcm-router/unix-api-server.hpp"

#include <sys/socket.h>
#include <sys/un.h>
#include <unistd.h>

#include <filesystem>
#include <thread>

namespace fs = std::filesystem;

namespace homepi::pcm_router {

UnixApiServer::UnixApiServer(std::string socket_path, SnapshotLineFn snapshot_line_fn,
                             SocketCommandFn on_command)
    : socket_path_(std::move(socket_path)),
      snapshot_line_fn_(std::move(snapshot_line_fn)),
      on_command_(std::move(on_command)) {}

UnixApiServer::~UnixApiServer() { stop(); }

bool UnixApiServer::start() {
  if (server_fd_ >= 0) {
    return true;
  }
  std::error_code ec;
  fs::remove(socket_path_, ec);
  fs::create_directories(fs::path(socket_path_).parent_path(), ec);

  server_fd_ = socket(AF_UNIX, SOCK_STREAM, 0);
  if (server_fd_ < 0) {
    return false;
  }

  sockaddr_un addr{};
  addr.sun_family = AF_UNIX;
  if (socket_path_.size() >= sizeof(addr.sun_path)) {
    close(server_fd_);
    server_fd_ = -1;
    return false;
  }
  std::snprintf(addr.sun_path, sizeof(addr.sun_path), "%s", socket_path_.c_str());
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
  {
    std::lock_guard lock(clients_mutex_);
    for (int fd : subscribers_) {
      shutdown(fd, SHUT_RDWR);
      close(fd);
    }
    subscribers_.clear();
    for (int fd : active_clients_) {
      shutdown(fd, SHUT_RDWR);
      close(fd);
    }
    active_clients_.clear();
  }
  if (thread_.joinable()) {
    thread_.join();
  }
  std::error_code ec;
  fs::remove(socket_path_, ec);
}

void UnixApiServer::broadcast(const std::string& ndjson_line) {
  const std::string frame = ndjson_line + "\n";
  std::lock_guard lock(clients_mutex_);
  for (auto it = subscribers_.begin(); it != subscribers_.end();) {
    if (write(*it, frame.c_str(), frame.size()) < 0) {
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
  while (!stop_.load()) {
    const ssize_t n = read(client_fd, chunk, sizeof(chunk));
    if (n <= 0) {
      break;
    }
    buffer.append(chunk, static_cast<size_t>(n));
    size_t pos = 0;
    while ((pos = buffer.find('\n')) != std::string::npos) {
      const std::string line = buffer.substr(0, pos);
      buffer.erase(0, pos + 1);
      if (line.find("\"subscribe\"") != std::string::npos) {
        if (!subscribed) {
          subscribed = true;
          std::lock_guard lock(clients_mutex_);
          subscribers_.insert(client_fd);
        }
        std::string correlation_id = "connect";
        const std::string key = "\"correlationId\"";
        const auto pos = line.find(key);
        if (pos != std::string::npos) {
          const auto quote_start = line.find('"', pos + key.size());
          if (quote_start != std::string::npos) {
            const auto quote_end = line.find('"', quote_start + 1);
            if (quote_end != std::string::npos) {
              correlation_id = line.substr(quote_start + 1, quote_end - quote_start - 1);
            }
          }
        }
        if (snapshot_line_fn_) {
          const std::string response = snapshot_line_fn_(correlation_id) + "\n";
          write(client_fd, response.c_str(), response.size());
        }
        continue;
      }
      if (on_command_) {
        std::string method = "unknown";
        if (line.find("route_start") != std::string::npos) {
          method = "route_start";
        } else if (line.find("route_end") != std::string::npos) {
          method = "route_end";
        } else if (line.find("route_join") != std::string::npos) {
          method = "route_join";
        } else if (line.find("set_routing") != std::string::npos) {
          method = "set_routing";
        }
        if (method != "unknown") {
          on_command_(method, 0, "socket", line);
          if (snapshot_line_fn_) {
            const std::string response = snapshot_line_fn_("routing") + "\n";
            write(client_fd, response.c_str(), response.size());
          }
          break;
        }
      }
    }
  }
  std::lock_guard lock(clients_mutex_);
  subscribers_.erase(client_fd);
  active_clients_.erase(client_fd);
  close(client_fd);
}

}  // namespace homepi::pcm_router
