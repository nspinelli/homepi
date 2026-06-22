#include "homepi/transport/latest-value-publisher.hpp"

#include <sys/socket.h>
#include <unistd.h>

#include <cerrno>
#include <cstring>

namespace homepi::transport {

void LatestValuePublisher::add_client(int fd) {
  std::lock_guard lock(mutex_);
  clients_[fd] = ClientState{};
}

void LatestValuePublisher::remove_client(int fd) {
  std::lock_guard lock(mutex_);
  clients_.erase(fd);
}

void LatestValuePublisher::publish(std::string frame) {
  std::lock_guard lock(mutex_);
  latest_frame_ = std::move(frame);
  for (auto& [fd, state] : clients_) {
    (void)fd;
    state.pending = latest_frame_;
    state.offset = 0;
  }
}

bool LatestValuePublisher::handle_writable(int fd) {
  std::lock_guard lock(mutex_);
  const auto it = clients_.find(fd);
  if (it == clients_.end() || it->second.pending.empty()) {
    return true;
  }

  ClientState& state = it->second;
  while (state.offset < state.pending.size()) {
    const ssize_t written =
        ::write(fd, state.pending.data() + state.offset,
                state.pending.size() - state.offset);
    if (written > 0) {
      state.offset += static_cast<std::size_t>(written);
      continue;
    }
    if (written < 0 && (errno == EAGAIN || errno == EWOULDBLOCK)) {
      return false;
    }
    clients_.erase(it);
    close(fd);
    return true;
  }

  state.pending.clear();
  state.offset = 0;
  return true;
}

std::vector<int> LatestValuePublisher::clients_with_pending() const {
  std::lock_guard lock(mutex_);
  std::vector<int> fds;
  fds.reserve(clients_.size());
  for (const auto& [fd, state] : clients_) {
    if (!state.pending.empty() && state.offset < state.pending.size()) {
      fds.push_back(fd);
    }
  }
  return fds;
}

}  // namespace homepi::transport
