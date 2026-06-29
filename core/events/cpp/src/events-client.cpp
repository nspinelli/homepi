#include "homepi/events/events-client.hpp"

#include "homepi/events/broker-protocol.hpp"
#include "homepi/ndjson.hpp"
#include "homepi/transport/unix-socket.hpp"

#include <sys/socket.h>
#include <unistd.h>

#include <chrono>
#include <sstream>
#include <thread>

namespace homepi::events {

EventsClient::EventsClient(std::string socket_path, std::string source)
    : socket_path_(std::move(socket_path)), source_(std::move(source)) {}

void EventsClient::start(const std::vector<std::string>& subscribes,
                         const std::vector<std::string>& publishes,
                         EventsClientMessageFn on_message) {
  subscribes_ = subscribes;
  publishes_ = publishes;
  on_message_ = std::move(on_message);
  stop_ = false;
  std::thread([this]() { connect_loop(); }).detach();
}

void EventsClient::stop() {
  stop_ = true;
  if (fd_ >= 0) {
    shutdown(fd_, SHUT_RDWR);
    close(fd_);
    fd_ = -1;
  }
}

std::string EventsClient::build_register_line(
    const std::vector<std::string>& subscribes,
    const std::vector<std::string>& publishes) const {
  std::ostringstream out;
  out << "{\"method\":\"register\",\"source\":\"" << source_ << "\",\"subscribes\":[";
  for (std::size_t i = 0; i < subscribes.size(); ++i) {
    if (i > 0) {
      out << ',';
    }
    out << '"' << subscribes[i] << '"';
  }
  out << "],\"publishes\":[";
  for (std::size_t i = 0; i < publishes.size(); ++i) {
    if (i > 0) {
      out << ',';
    }
    out << '"' << publishes[i] << '"';
  }
  out << "]}";
  return out.str();
}

void EventsClient::connect_loop() {
  while (!stop_) {
    if (!ensure_connected()) {
      std::this_thread::sleep_for(std::chrono::seconds(1));
      continue;
    }

    char chunk[4096];
    const ssize_t n = read(fd_, chunk, sizeof(chunk));
    if (n <= 0) {
      close(fd_);
      fd_ = -1;
      std::this_thread::sleep_for(std::chrono::seconds(1));
      continue;
    }

    read_buffer_.append(chunk, static_cast<std::size_t>(n));
    const auto [lines, remainder] = homepi::transport::split_ndjson_lines(read_buffer_);
    read_buffer_ = remainder;
    for (const std::string& line : lines) {
      if (is_v2_broker_socket(socket_path_)) {
        if (const auto legacy = broker_wire_to_legacy_envelope(line)) {
          if (on_message_) {
            on_message_(*legacy);
          }
        }
        continue;
      }
      if (on_message_) {
        on_message_(line);
      }
    }
  }
}

bool EventsClient::ensure_connected() {
  if (fd_ >= 0) {
    return true;
  }
  fd_ = homepi::transport::connect_unix_stream_socket(socket_path_);
  if (fd_ < 0) {
    return false;
  }
  if (is_v2_broker_socket(socket_path_)) {
    if (!subscribes_.empty()) {
      return write_line(build_broker_subscribe_line(source_, subscribes_));
    }
    return true;
  }
  return write_line(build_register_line(subscribes_, publishes_));
}

bool EventsClient::write_line(const std::string& line) {
  if (fd_ < 0) {
    return false;
  }
  const std::string frame = line + "\n";
  std::size_t offset = 0;
  while (offset < frame.size()) {
    const ssize_t written = write(fd_, frame.data() + offset, frame.size() - offset);
    if (written <= 0) {
      return false;
    }
    offset += static_cast<std::size_t>(written);
  }
  return true;
}

bool EventsClient::publish(const std::string& ndjson_line) {
  if (!ensure_connected()) {
    return false;
  }
  if (is_v2_broker_socket(socket_path_)) {
    return write_line(build_broker_publish_line(source_, ndjson_line));
  }
  return write_line(ndjson_line);
}

}  // namespace homepi::events
