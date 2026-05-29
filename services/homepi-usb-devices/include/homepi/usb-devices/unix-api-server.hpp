#pragma once

#include <atomic>
#include <functional>
#include <memory>
#include <string>
#include <thread>

#include "homepi/usb-devices/types.hpp"

namespace homepi::usb_devices {

class AssignmentRepository;
class ArtifactWriter;

/** Request handler context for socket API commands. */
struct ApiContext {
  AssignmentRepository* repository = nullptr;
  ArtifactWriter* artifacts = nullptr;
  ServiceConfig config{};
  std::function<std::vector<UsbDevice>()> scan_fn;
  std::function<void()> on_devices_changed;
  std::function<ServiceHealth()> health_fn;
};

/** Unix domain socket NDJSON API server. */
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

 private:
  void listen_loop();
  std::string handle_request(const std::string& line) const;

  ApiContext context_;
  std::thread thread_;
  std::atomic<bool> stop_{false};
  int server_fd_ = -1;
};

}  // namespace homepi::usb_devices
