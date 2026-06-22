#pragma once

#include <atomic>
#include <cstdint>
#include <mutex>
#include <string>
#include <unordered_map>
#include <vector>

namespace homepi::events {

/** Registered broker client state. */
struct BrokerClient {
  int fd = -1;
  std::string source;
  std::vector<std::string> subscribes;
  std::vector<std::string> publishes;
  bool registered = false;
  std::string read_buffer;
};

/**
 * HomePi control-plane event broker bound to /run/homepi/events.sock.
 */
class EventBroker {
 public:
  /**
   * Creates a broker for the given socket path.
   * @param socket_path Unix socket path.
   */
  explicit EventBroker(std::string socket_path);

  /** Starts listening and blocks until stop() is called. */
  void run();

  /** Requests broker shutdown. */
  void stop();

 private:
  bool topic_matches(const std::string& pattern, const std::string& topic) const;
  bool validate_envelope_fields(const std::string& line) const;
  std::string extract_json_string(const std::string& json, const std::string& key) const;
  void handle_line(int fd, const std::string& line);
  void handle_register(int fd, const std::string& line);
  void handle_subscribe(int fd, const std::string& line);
  void route_event(int sender_fd, const std::string& line);
  void remove_client(int fd);
  void fanout(const std::string& line, const std::string& topic, int exclude_fd);
  void listen_loop();

  std::string socket_path_;
  std::atomic<bool> stop_{false};
  int listen_fd_ = -1;
  mutable std::mutex mutex_;
  std::unordered_map<int, BrokerClient> clients_;
};

}  // namespace homepi::events
