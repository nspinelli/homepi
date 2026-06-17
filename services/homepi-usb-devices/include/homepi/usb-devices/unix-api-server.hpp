#pragma once

#include <atomic>
#include <functional>
#include <mutex>
#include <set>
#include <string>
#include <thread>

#include "homepi/usb-devices/types.hpp"

namespace homepi::usb_devices {

class AssignmentRepository;
class ArtifactWriter;
class AudioProfileService;

/** Request handler context for socket API commands. */
struct ApiContext {
  AssignmentRepository* repository = nullptr;
  ArtifactWriter* artifacts = nullptr;
  AudioProfileService* audio_profiles = nullptr;
  ServiceConfig config{};
  std::function<std::vector<UsbDevice>()> scan_fn;
  std::function<void()> on_devices_changed;
  std::function<ServiceHealth()> health_fn;
  /** Called when a client subscribes to the event stream. */
  std::function<void()> on_subscribe_fn;
};

/** Unix domain socket NDJSON API server with RPC and event fan-out. */
class UnixApiServer {
 public:
  explicit UnixApiServer(ApiContext context);
  ~UnixApiServer();

  UnixApiServer(const UnixApiServer&) = delete;
  UnixApiServer& operator=(const UnixApiServer&) = delete;

  /**
   * Starts listening on the configured socket path.
   * @return True when listening.
   */
  bool start();

  /** Stops the server and removes the socket file. */
  void stop();

  /**
   * Broadcasts an event line to all subscribers.
   * @param line NDJSON event envelope.
   */
  void broadcast(const std::string& line);

 private:
  void listen_loop();
  void handle_client(int client_fd);
  std::string handle_request(const std::string& line) const;

  ApiContext context_;
  std::thread thread_;
  std::atomic<bool> stop_{false};
  int server_fd_ = -1;
  std::mutex clients_mutex_;
  std::set<int> subscribers_;
};

}  // namespace homepi::usb_devices
