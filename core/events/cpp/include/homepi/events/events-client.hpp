#pragma once

#include <functional>
#include <string>
#include <vector>

namespace homepi::events {

/** Callback invoked for each received event envelope line. */
using EventsClientMessageFn = std::function<void(const std::string& ndjson_line)>;

/**
 * Persistent client connection to the HomePi events broker.
 */
class EventsClient {
 public:
  /**
   * Creates a client for the given socket path.
   * @param socket_path Broker socket path.
   * @param source Service source name used for registration.
   */
  EventsClient(std::string socket_path, std::string source);

  /**
   * Starts the reconnect loop and registers with the broker.
   * @param subscribes Topic patterns to subscribe to.
   * @param publishes Topics this client may publish.
   * @param on_message Handler for inbound event envelopes.
   */
  void start(const std::vector<std::string>& subscribes,
             const std::vector<std::string>& publishes,
             EventsClientMessageFn on_message);

  /** Stops the client loop. */
  void stop();

  /**
   * Publishes an event envelope line to the broker.
   * @param ndjson_line Complete envelope without trailing newline.
   * @returns True when written to an active connection.
   */
  bool publish(const std::string& ndjson_line);

 private:
  void connect_loop();
  bool ensure_connected();
  bool write_line(const std::string& line);
  std::string build_register_line(const std::vector<std::string>& subscribes,
                                  const std::vector<std::string>& publishes) const;

  std::string socket_path_;
  std::string source_;
  EventsClientMessageFn on_message_;
  std::vector<std::string> subscribes_;
  std::vector<std::string> publishes_;
  int fd_ = -1;
  bool stop_ = false;
  std::string read_buffer_;
};

}  // namespace homepi::events
