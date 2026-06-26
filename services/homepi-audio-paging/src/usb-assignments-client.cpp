#include "homepi/audio-paging/usb-assignments-client.hpp"

#include <sys/socket.h>
#include <sys/un.h>
#include <unistd.h>

#include <cstring>

#include "homepi/audio-paging/json-utils.hpp"

namespace homepi::audio_paging {

namespace {

int connect_unix_socket(const std::string& path) {
  const int fd = socket(AF_UNIX, SOCK_STREAM, 0);
  if (fd < 0) {
    return -1;
  }
  sockaddr_un address{};
  address.sun_family = AF_UNIX;
  if (path.size() >= sizeof(address.sun_path)) {
    close(fd);
    return -1;
  }
  std::strncpy(address.sun_path, path.c_str(), sizeof(address.sun_path) - 1);
  if (connect(fd, reinterpret_cast<sockaddr*>(&address), sizeof(address)) < 0) {
    close(fd);
    return -1;
  }
  return fd;
}

}  // namespace

UsbAssignmentsClient::UsbAssignmentsClient(std::string socket_path)
    : socket_path_(std::move(socket_path)) {}

std::optional<UsbAssignmentsSnapshot> UsbAssignmentsClient::get_assignments() const {
  const int fd = connect_unix_socket(socket_path_);
  if (fd < 0) {
    return std::nullopt;
  }
  const std::string request =
      "{\"method\":\"getAssignments\",\"correlationId\":\"audio-paging-read\"}\n";
  if (write(fd, request.c_str(), request.size()) < 0) {
    close(fd);
    return std::nullopt;
  }
  char buffer[8192];
  const ssize_t bytes = read(fd, buffer, sizeof(buffer) - 1);
  close(fd);
  if (bytes <= 0) {
    return std::nullopt;
  }
  buffer[bytes] = '\0';
  const std::string line(buffer);
  const std::string data = json_get_object(line, "data");
  UsbAssignmentsSnapshot snapshot;
  const std::string serial = json_get_string(data, "serial");
  const std::string audio_primary = json_get_string(data, "audioPrimary");
  const std::string paging = json_get_string(data, "paging");
  if (!serial.empty()) {
    snapshot.serial = serial;
  }
  if (!audio_primary.empty()) {
    snapshot.audio_primary = audio_primary;
  }
  if (!paging.empty()) {
    snapshot.paging = paging;
  }
  return snapshot;
}

}  // namespace homepi::audio_paging
