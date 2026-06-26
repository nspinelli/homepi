#pragma once

#include <atomic>
#include <functional>
#include <mutex>
#include <set>
#include <string>
#include <thread>

#include "homepi/audio-paging/types.hpp"

namespace homepi::audio_paging {

class PagingRepository;

/** Dependencies used by unix-api-server request handlers. */
struct UnixApiContext {
  PagingRepository* repository = nullptr;
  std::function<PagingStatus()> status_fn;
  std::function<void(const PagingConfig& config)> on_config_updated;
  std::function<bool(const std::string& voice_id)> on_reload_voice;
  std::function<bool(const std::string& text, const std::string& voice_id)> on_preview_voice;
};

/** Unix domain socket RPC server for paging status/config/voice/chime management. */
class UnixApiServer {
 public:
  /** Creates server bound to socket path and request context. */
  UnixApiServer(std::string socket_path, UnixApiContext context);

  ~UnixApiServer();

  UnixApiServer(const UnixApiServer&) = delete;
  UnixApiServer& operator=(const UnixApiServer&) = delete;

  /** Starts accept loop and listens for rpc requests. */
  bool start();

  /** Stops server and removes socket path. */
  void stop();

  /** Broadcasts ndjson event line to active subscribers. */
  void broadcast(const std::string& line);

 private:
  void listen_loop();
  void handle_client(int fd);
  std::string handle_request(const std::string& line);
  std::string ok_response(const std::string& correlation_id, const std::string& data_json) const;
  std::string error_response(const std::string& correlation_id, const std::string& code,
                             const std::string& message) const;

  std::string socket_path_;
  std::string socket_dir_;
  UnixApiContext context_;
  std::thread server_thread_;
  std::atomic<bool> stop_{false};
  int server_fd_ = -1;
  std::mutex clients_mutex_;
  std::set<int> subscribers_;
};

}  // namespace homepi::audio_paging
