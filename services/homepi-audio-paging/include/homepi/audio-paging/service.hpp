#pragma once

#include <memory>

#include "homepi/audio-paging/service-config.hpp"
#include "homepi/audio-paging/types.hpp"

namespace homepi::audio_paging {

class ChimePlayer;
class EventsSubscriber;
class HifiPagingController;
class Orchestrator;
class PagingRepository;
class PcmPlayer;
class PiperWorker;
class ResourceManager;
class UnixApiServer;
class UsbAssignmentsClient;

/** Top-level service wiring for homepi-audio-paging daemon. */
class Service {
 public:
  /** Creates service with resolved runtime config. */
  explicit Service(ServiceConfig config);
  ~Service();

  Service(const Service&) = delete;
  Service& operator=(const Service&) = delete;

  /** Runs initialization and blocks until process shutdown. */
  int run();

  /** Requests shutdown and stops workers/socket server. */
  void shutdown();

 private:
  void refresh_readiness();
  PagingStatus build_status() const;

  ServiceConfig config_;
  std::unique_ptr<PagingRepository> repository_;
  std::unique_ptr<UsbAssignmentsClient> usb_client_;
  std::unique_ptr<PiperWorker> piper_worker_;
  std::unique_ptr<PcmPlayer> pcm_player_;
  std::unique_ptr<ChimePlayer> chime_player_;
  std::unique_ptr<HifiPagingController> hifi_controller_;
  std::unique_ptr<ResourceManager> resource_manager_;
  std::unique_ptr<Orchestrator> orchestrator_;
  std::unique_ptr<UnixApiServer> unix_api_;
  std::unique_ptr<EventsSubscriber> events_subscriber_;
  PagingStatus status_;
};

}  // namespace homepi::audio_paging
