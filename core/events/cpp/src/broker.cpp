#include "homepi/events/broker.hpp"

#include "homepi/ndjson.hpp"
#include "homepi/transport/unix-socket.hpp"

#include <sys/socket.h>
#include <unistd.h>

#include <cerrno>
#include <cstring>
#include <iostream>
#include <thread>

namespace homepi::events {

namespace {

constexpr std::size_t kMaxMessageBytes = 65536;

bool write_all(int fd, const std::string& data) {
  std::size_t offset = 0;
  while (offset < data.size()) {
    const ssize_t written =
        ::send(fd, data.data() + offset, data.size() - offset, MSG_NOSIGNAL);
    if (written <= 0) {
      return false;
    }
    offset += static_cast<std::size_t>(written);
  }
  return true;
}

std::vector<std::string> extract_string_array(const std::string& json,
                                              const std::string& key) {
  std::vector<std::string> values;
  const std::string needle = "\"" + key + "\":";
  const std::size_t key_pos = json.find(needle);
  if (key_pos == std::string::npos) {
    return values;
  }
  const std::size_t start = json.find('[', key_pos);
  const std::size_t end = json.find(']', start);
  if (start == std::string::npos || end == std::string::npos || end <= start) {
    return values;
  }
  const std::string slice = json.substr(start + 1, end - start - 1);
  std::size_t pos = 0;
  while (pos < slice.size()) {
    const std::size_t q1 = slice.find('"', pos);
    if (q1 == std::string::npos) {
      break;
    }
    const std::size_t q2 = slice.find('"', q1 + 1);
    if (q2 == std::string::npos) {
      break;
    }
    values.push_back(slice.substr(q1 + 1, q2 - q1 - 1));
    pos = q2 + 1;
  }
  return values;
}

}  // namespace

EventBroker::EventBroker(std::string socket_path)
    : socket_path_(std::move(socket_path)) {}

void EventBroker::stop() {
  stop_ = true;
  if (listen_fd_ >= 0) {
    shutdown(listen_fd_, SHUT_RDWR);
  }
}

void EventBroker::run() {
  listen_fd_ = homepi::transport::create_listening_unix_stream_socket(socket_path_);
  if (listen_fd_ < 0) {
    std::cerr << "homepi-events: failed to bind " << socket_path_ << "\n";
    return;
  }
  homepi::transport::apply_socket_permissions(socket_path_, 0660, "homepi");
  listen_loop();
  homepi::transport::remove_socket_path(socket_path_);
  if (listen_fd_ >= 0) {
    close(listen_fd_);
    listen_fd_ = -1;
  }
}

void EventBroker::listen_loop() {
  while (!stop_.load()) {
    const int client_fd = accept(listen_fd_, nullptr, nullptr);
    if (client_fd < 0) {
      if (stop_.load()) {
        break;
      }
      if (errno == EINTR) {
        continue;
      }
      continue;
    }

    {
      std::lock_guard lock(mutex_);
      clients_[client_fd] = BrokerClient{.fd = client_fd};
    }

    std::thread([this, client_fd]() {
      char chunk[4096];
      while (!stop_.load()) {
        const ssize_t n = read(client_fd, chunk, sizeof(chunk));
        if (n <= 0) {
          break;
        }
        std::string buffer;
        {
          std::lock_guard lock(mutex_);
          auto it = clients_.find(client_fd);
          if (it == clients_.end()) {
            break;
          }
          it->second.read_buffer.append(chunk, static_cast<std::size_t>(n));
          buffer = it->second.read_buffer;
        }

        const auto [lines, remainder] = homepi::transport::split_ndjson_lines(buffer);
        {
          std::lock_guard lock(mutex_);
          auto it = clients_.find(client_fd);
          if (it != clients_.end()) {
            it->second.read_buffer = remainder;
          }
        }
        for (const std::string& line : lines) {
          if (line.size() > kMaxMessageBytes) {
            continue;
          }
          handle_line(client_fd, line);
        }
      }
      remove_client(client_fd);
      close(client_fd);
    }).detach();
  }
}

bool EventBroker::topic_matches(const std::string& pattern,
                                const std::string& topic) const {
  if (pattern == topic) {
    return true;
  }
  if (!pattern.empty() && pattern.back() == '*') {
    const std::string prefix = pattern.substr(0, pattern.size() - 1);
    return topic.rfind(prefix, 0) == 0;
  }
  return false;
}

std::string EventBroker::extract_json_string(const std::string& json,
                                             const std::string& key) const {
  const std::string needle = "\"" + key + "\":\"";
  const std::size_t pos = json.find(needle);
  if (pos == std::string::npos) {
    return {};
  }
  const std::size_t start = pos + needle.size();
  const std::size_t end = json.find('"', start);
  if (end == std::string::npos) {
    return {};
  }
  return json.substr(start, end - start);
}

bool EventBroker::validate_envelope_fields(const std::string& line) const {
  return !extract_json_string(line, "source").empty() &&
         !extract_json_string(line, "topic").empty() &&
         !extract_json_string(line, "event").empty() &&
         line.find("\"payload\"") != std::string::npos;
}

void EventBroker::handle_line(int fd, const std::string& line) {
  if (line.find("\"method\":\"register\"") != std::string::npos) {
    handle_register(fd, line);
    return;
  }
  if (line.find("\"method\":\"subscribe\"") != std::string::npos) {
    handle_subscribe(fd, line);
    return;
  }
  if (line.find("\"topic\"") != std::string::npos && line.find("\"event\"") != std::string::npos) {
    route_event(fd, line);
  }
}

void EventBroker::handle_register(int fd, const std::string& line) {
  const std::string source = extract_json_string(line, "source");
  if (source.empty()) {
    return;
  }
  std::lock_guard lock(mutex_);
  auto it = clients_.find(fd);
  if (it == clients_.end()) {
    return;
  }
  it->second.source = source;
  it->second.subscribes = extract_string_array(line, "subscribes");
  it->second.publishes = extract_string_array(line, "publishes");
  it->second.registered = true;
}

void EventBroker::handle_subscribe(int fd, const std::string& line) {
  const auto topics = extract_string_array(line, "topics");
  if (topics.empty()) {
    return;
  }
  std::lock_guard lock(mutex_);
  auto it = clients_.find(fd);
  if (it == clients_.end()) {
    return;
  }
  for (const std::string& topic : topics) {
    it->second.subscribes.push_back(topic);
  }
}

void EventBroker::route_event(int sender_fd, const std::string& line) {
  if (!validate_envelope_fields(line)) {
    return;
  }
  const std::string topic = extract_json_string(line, "topic");
  const std::string source = extract_json_string(line, "source");

  {
    std::lock_guard lock(mutex_);
    const auto sender = clients_.find(sender_fd);
    if (sender != clients_.end() && sender->second.registered &&
        !sender->second.publishes.empty()) {
      bool allowed = false;
      for (const std::string& pattern : sender->second.publishes) {
        if (topic_matches(pattern, topic)) {
          allowed = true;
          break;
        }
      }
      if (!allowed && sender->second.source != source) {
        return;
      }
    }
  }

  fanout(line, topic, sender_fd);
}

void EventBroker::fanout(const std::string& line, const std::string& topic,
                         int exclude_fd) {
  const std::string frame = line + "\n";
  std::vector<int> targets;
  {
    std::lock_guard lock(mutex_);
    for (const auto& [fd, client] : clients_) {
      if (fd == exclude_fd || !client.registered) {
        continue;
      }
      for (const std::string& pattern : client.subscribes) {
        if (topic_matches(pattern, topic)) {
          targets.push_back(fd);
          break;
        }
      }
    }
  }
  for (int fd : targets) {
    if (!write_all(fd, frame)) {
      remove_client(fd);
      close(fd);
    }
  }
}

void EventBroker::remove_client(int fd) {
  std::lock_guard lock(mutex_);
  clients_.erase(fd);
}

}  // namespace homepi::events
