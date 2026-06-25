#include "homepi/audio-orchestrator/service-socket-client.hpp"

#include "homepi/audio-orchestrator/json-utils.hpp"
#include "homepi/transport/unix-socket.hpp"

#include <arpa/inet.h>
#include <netinet/in.h>
#include <sys/socket.h>
#include <sys/time.h>
#include <unistd.h>

#include <cstring>
#include <iostream>
#include <sstream>
#include <thread>

namespace homepi::audio_orchestrator {

namespace {

bool write_all(int fd, const std::string& data) {
  std::size_t offset = 0;
  while (offset < data.size()) {
    const ssize_t written = write(fd, data.data() + offset, data.size() - offset);
    if (written <= 0) {
      return false;
    }
    offset += static_cast<std::size_t>(written);
  }
  return true;
}

std::string read_line(int fd) {
  std::string line;
  char ch = '\0';
  while (read(fd, &ch, 1) == 1) {
    if (ch == '\n') {
      break;
    }
    line.push_back(ch);
    if (line.size() > 65536) {
      break;
    }
  }
  return line;
}

void set_socket_timeouts(int fd, int seconds) {
  timeval tv{};
  tv.tv_sec = seconds;
  tv.tv_usec = 0;
  setsockopt(fd, SOL_SOCKET, SO_RCVTIMEO, &tv, sizeof(tv));
  setsockopt(fd, SOL_SOCKET, SO_SNDTIMEO, &tv, sizeof(tv));
}

}  // namespace

ServiceSocketClient::ServiceSocketClient(SocketPaths paths) : paths_(std::move(paths)) {}

std::string ServiceSocketClient::pcm_route(const std::string& method, int zone_id) const {
  const int fd = homepi::transport::connect_unix_stream_socket(paths_.pcm_router);
  if (fd < 0) {
    return {};
  }
  set_socket_timeouts(fd, 3);

  std::ostringstream request;
  request << "{\"method\":\"" << method << "\",\"zoneId\":" << zone_id << "}\n";
  if (!write_all(fd, request.str())) {
    close(fd);
    return {};
  }

  const std::string response = read_line(fd);
  close(fd);
  return response;
}

int ServiceSocketClient::pcm_owner_from_response(const std::string& response) const {
  if (response.empty()) {
    return 0;
  }
  const std::string payload = parse_payload_json(response);
  return parse_int_field(payload, "ownerZoneId");
}

void ServiceSocketClient::send_hifi_command(const std::string& command) const {
  const int fd = homepi::transport::connect_unix_stream_socket(paths_.hifi_serial);
  if (fd < 0) {
    return;
  }

  std::ostringstream request;
  request << "{\"method\":\"sendCommand\",\"correlationId\":\"audio-orchestrator\","
          << "\"command\":\"" << command << "\"}\n";
  write_all(fd, request.str());
  close(fd);
}

void ServiceSocketClient::send_hifi_command_async(const std::string& command) const {
  std::thread([this, command]() { send_hifi_command(command); }).detach();
}

void ServiceSocketClient::execute_hifi_command_async(const std::string& event,
                                                     const std::string& payload_json) const {
  std::thread([this, event, payload_json]() {
    const int fd = homepi::transport::connect_unix_stream_socket(paths_.hifi_serial);
    if (fd < 0) {
      return;
    }
    set_socket_timeouts(fd, 3);

    std::ostringstream request;
    request << "{\"method\":\"executeHifiCommand\",\"correlationId\":\"audio-orchestrator\","
            << "\"event\":\"" << event << "\"";
    if (!payload_json.empty()) {
      request << ',' << payload_json;
    }
    request << "}\n";
    if (!write_all(fd, request.str())) {
      close(fd);
      return;
    }
    (void)read_line(fd);
    close(fd);
  }).detach();
}

void ServiceSocketClient::nqptp_play_begin() const {
  const int fd = socket(AF_INET, SOCK_DGRAM, 0);
  if (fd < 0) {
    return;
  }

  sockaddr_in addr{};
  addr.sin_family = AF_INET;
  addr.sin_port = htons(static_cast<uint16_t>(paths_.nqptp_port));
  if (inet_pton(AF_INET, paths_.nqptp_host.c_str(), &addr.sin_addr) != 1) {
    close(fd);
    return;
  }

  const char* message = "/nqptp B\n";
  sendto(fd, message, std::strlen(message), 0, reinterpret_cast<sockaddr*>(&addr), sizeof(addr));
  close(fd);
}

}  // namespace homepi::audio_orchestrator
