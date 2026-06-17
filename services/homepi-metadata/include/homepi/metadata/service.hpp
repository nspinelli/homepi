#pragma once

#include <memory>
#include <string>

#include "homepi/metadata/service-config.hpp"

namespace homepi::metadata {

class MetadataStateRepository;
class NowPlayingState;
class PipeManager;
class PcmRouterSubscriber;
class UnixApiServer;

namespace events {
class EventEmitter;
}

/** Main metadata daemon orchestrating pipe drain, PCM owner tracking, and events. */
class Service {
 public:
  explicit Service(ServiceConfig config);
  ~Service();

  Service(const Service&) = delete;
  Service& operator=(const Service&) = delete;

  /**
   * Runs the metadata service until SIGINT/SIGTERM.
   * @returns Process exit code.
   */
  int run();

  /** Stops background workers and releases resources. */
  void shutdown();

 private:
  struct Impl;
  std::unique_ptr<Impl> impl_;
  ServiceConfig config_;
};

}  // namespace homepi::metadata
