#include "homepi/transport/unix-socket.hpp"

#include <fcntl.h>
#include <grp.h>
#include <sys/socket.h>
#include <sys/stat.h>
#include <sys/un.h>
#include <unistd.h>

#include <cerrno>
#include <cstring>
#include <filesystem>

namespace fs = std::filesystem;

namespace homepi::transport {

namespace {

bool prepare_socket_directory(const std::string& path) {
  const fs::path parent = fs::path(path).parent_path();
  if (parent.empty()) {
    return true;
  }
  std::error_code ec;
  fs::create_directories(parent, ec);
  return !ec;
}

}  // namespace

int create_listening_unix_stream_socket(const std::string& path, int backlog) {
  if (path.size() >= sizeof(sockaddr_un::sun_path)) {
    return -1;
  }
  prepare_socket_directory(path);
  remove_socket_path(path);

  const int fd = socket(AF_UNIX, SOCK_STREAM, 0);
  if (fd < 0) {
    return -1;
  }

  sockaddr_un addr{};
  addr.sun_family = AF_UNIX;
  std::snprintf(addr.sun_path, sizeof(addr.sun_path), "%s", path.c_str());
  if (bind(fd, reinterpret_cast<sockaddr*>(&addr), sizeof(addr)) < 0) {
    close(fd);
    return -1;
  }
  if (listen(fd, backlog) < 0) {
    close(fd);
    return -1;
  }
  return fd;
}

int connect_unix_stream_socket(const std::string& path) {
  if (path.size() >= sizeof(sockaddr_un::sun_path)) {
    return -1;
  }
  const int fd = socket(AF_UNIX, SOCK_STREAM, 0);
  if (fd < 0) {
    return -1;
  }

  sockaddr_un addr{};
  addr.sun_family = AF_UNIX;
  std::snprintf(addr.sun_path, sizeof(addr.sun_path), "%s", path.c_str());
  if (::connect(fd, reinterpret_cast<sockaddr*>(&addr), sizeof(addr)) < 0) {
    close(fd);
    return -1;
  }
  return fd;
}

bool set_nonblocking(int fd) {
  const int flags = fcntl(fd, F_GETFL, 0);
  if (flags < 0) {
    return false;
  }
  return fcntl(fd, F_SETFL, flags | O_NONBLOCK) == 0;
}

bool apply_socket_permissions(const std::string& path, unsigned int mode,
                              const std::string& group_name) {
  if (chmod(path.c_str(), mode) != 0) {
    return false;
  }
  if (group_name.empty()) {
    return true;
  }
  const group* grp = getgrnam(group_name.c_str());
  if (grp == nullptr) {
    return false;
  }
  return chown(path.c_str(), static_cast<uid_t>(-1), grp->gr_gid) == 0;
}

void remove_socket_path(const std::string& path) {
  std::error_code ec;
  fs::remove(path, ec);
}

}  // namespace homepi::transport
