#include "homepi/transport/latest-value-publisher.hpp"

#include <cassert>
#include <fcntl.h>
#include <iostream>
#include <string>
#include <sys/socket.h>
#include <unistd.h>

int main() {
  int fds[2];
  if (socketpair(AF_UNIX, SOCK_STREAM, 0, fds) != 0) {
    return 1;
  }

  homepi::transport::LatestValuePublisher publisher;
  publisher.add_client(fds[1]);
  publisher.publish("frame-a");
  assert(publisher.handle_writable(fds[1]));

  char buffer[16] = {};
  const ssize_t n = read(fds[0], buffer, sizeof(buffer) - 1);
  assert(n > 0);
  assert(std::string(buffer, static_cast<std::size_t>(n)) == "frame-a");

  publisher.publish("frame-b");
  assert(publisher.handle_writable(fds[1]));
  char buffer2[16] = {};
  const ssize_t n2 = read(fds[0], buffer2, sizeof(buffer2) - 1);
  assert(n2 > 0);
  assert(std::string(buffer2, static_cast<std::size_t>(n2)) == "frame-b");

  close(fds[0]);
  close(fds[1]);
  std::cout << "ok\n";
  return 0;
}
