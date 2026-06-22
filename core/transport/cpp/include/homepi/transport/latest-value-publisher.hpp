#pragma once

#include <cstdint>
#include <mutex>
#include <string>
#include <unordered_map>
#include <vector>

namespace homepi::transport {

/**
 * Payload-agnostic latest-value fanout with at most one pending frame per client.
 */
class LatestValuePublisher {
 public:
  /** Adds a connected client socket. */
  void add_client(int fd);

  /** Removes a client socket. */
  void remove_client(int fd);

  /**
   * Overwrites the pending frame for all clients and enables write interest.
   * @param frame Complete frame bytes to deliver.
   */
  void publish(std::string frame);

  /**
   * Attempts to flush the pending frame for a client.
   * @param fd Client socket.
   * @returns True when the pending frame was fully written or absent.
   */
  bool handle_writable(int fd);

  /**
   * Returns client fds that still have pending frames.
   * @returns File descriptors with pending output.
   */
  std::vector<int> clients_with_pending() const;

 private:
  struct ClientState {
    std::string pending;
    std::size_t offset = 0;
  };

  mutable std::mutex mutex_;
  std::unordered_map<int, ClientState> clients_;
  std::string latest_frame_;
};

}  // namespace homepi::transport
