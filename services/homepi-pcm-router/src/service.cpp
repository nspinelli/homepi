#include "homepi/pcm-router/service.hpp"

#include <atomic>
#include <chrono>
#include <csignal>
#include <cstdlib>
#include <memory>
#include <sstream>
#include <thread>

#include "homepi/events/event-envelope.hpp"
#include "homepi/events/event-emitter.hpp"
#include "homepi/log.hpp"
#include "homepi/pcm-router/audio-bridge.hpp"
#include "homepi/pcm-router/audio-profile-loader.hpp"
#include "homepi/pcm-router/event-subscriber.hpp"
#include "homepi/pcm-router/json-utils.hpp"
#include "homepi/pcm-router/routing-state.hpp"
#include "homepi/pcm-router/snapshot-builder.hpp"
#include "homepi/pcm-router/unix-api-server.hpp"

namespace homepi::pcm_router {

namespace {

std::atomic<bool> g_stop{false};

void on_signal(int) { g_stop = true; }

void apply_routing(AudioBridge& bridge, RoutingState& routing) {
  bridge.apply_zone_modes(routing.zone_modes());
  bridge.set_playback_owner(routing.owner_zone_id());
}

}  // namespace

struct Service::Impl {
  RoutingState routing;
  AudioBridge bridge;
  ActiveAudioConfig active_config;
  std::unique_ptr<UnixApiServer> server;
  EventSubscriber profile_subscriber;
  homepi::events::EventEmitter* events = nullptr;

  explicit Impl(ServiceConfig config) : bridge(config) {}
};

Service::Service(ServiceConfig config)
    : config_(std::move(config)), impl_(std::make_unique<Impl>(config_)) {}

Service::~Service() = default;

std::string Service::build_snapshot_json() const {
  return SnapshotBuilder::build_payload(impl_->routing, impl_->bridge, impl_->active_config);
}

int Service::run() {
  std::signal(SIGINT, on_signal);
  std::signal(SIGTERM, on_signal);

  const auto log_level = config_.log_level == "DEBUG"   ? homepi::logging::LogLevel::DEBUG
                         : config_.log_level == "WARN" ? homepi::logging::LogLevel::WARN
                         : config_.log_level == "ERROR"
                             ? homepi::logging::LogLevel::ERROR
                             : homepi::logging::LogLevel::INFO;
  homepi::logging::Logger logger(config_.service, log_level);

  AudioProfileLoader loader(config_.database_path, config_.artifact_path);
  impl_->active_config = loader.load();

  homepi::events::EventEmitter events(config_.service, [&](const std::string& line) {
    if (impl_->server) {
      impl_->server->broadcast(line);
    }
  });
  impl_->events = &events;

  const auto emit_snapshot = [&](const std::string& correlation_id) {
    events.emit("modules.pcm.snapshot", "pcm_router_snapshot", correlation_id,
                build_snapshot_json());
  };

  impl_->server = std::make_unique<UnixApiServer>(
      config_.socket_path,
      [&](const std::string& correlation_id) {
        homepi::events::EventEnvelope envelope;
        envelope.id = config_.service + "-snapshot";
        envelope.source = config_.service;
        envelope.topic = "modules.pcm.snapshot";
        envelope.event = "pcm_router_snapshot";
        envelope.correlation_id = correlation_id;
        envelope.timestamp = homepi::events::iso_timestamp();
        envelope.payload_json = build_snapshot_json();
        return homepi::events::build_event_line(envelope);
      },
      [this, &emit_snapshot, &events](const std::string& method, int /*zone_id*/,
                             const std::string& correlation_id, const std::string& body_json) {
        if (method == "route_start") {
          const int zone_id = parse_int_field(body_json, "zoneId");
          const auto is_zone_active = [this](int stacked_zone_id) {
            return impl_->bridge.zone_recently_buffered(stacked_zone_id, 5000) ||
                   impl_->routing.zone_recently_routed(stacked_zone_id, 5000);
          };
          impl_->routing.on_route_start(zone_id, is_zone_active);
        } else if (method == "route_end") {
          const int zone_id = parse_int_field(body_json, "zoneId");
          impl_->routing.on_route_end(zone_id);
        } else if (method == "route_join") {
          impl_->routing.on_route_join(parse_int_field(body_json, "zoneId"));
        } else if (method == "set_routing") {
          impl_->routing.set_routing(parse_int_field(body_json, "ownerZoneId"),
                                     parse_int_array(body_json, "activeStack"));
        }
        apply_routing(impl_->bridge, impl_->routing);
        const std::string snapshot = build_snapshot_json();
        emit_snapshot(correlation_id.empty() ? "routing_changed" : correlation_id);
        std::ostringstream routing_payload;
        routing_payload << snapshot.substr(0, snapshot.size() - 1) << ",\"zoneId\":"
                        << parse_int_field(body_json, "zoneId") << ",\"method\":\"" << method
                        << "\"}";
        events.emit("modules.pcm", "routing_changed", correlation_id, routing_payload.str());
      });

  if (!impl_->server->start()) {
    logger.log(homepi::logging::LogLevel::ERROR, "pcm.router", "socket_start_failed", "startup",
               "failed to bind unix socket");
    return 1;
  }

  impl_->profile_subscriber.start(config_.usb_devices_socket,
                                  [this, &loader, &emit_snapshot](const std::string& event,
                                                                  const std::string& /*payload*/) {
                                    if (event == "audio_profile_paused" ||
                                        event == "audio_profile_invalid") {
                                      impl_->active_config.status = ProfileStatus::PausedInvalid;
                                      impl_->bridge.pause();
                                      emit_snapshot(event);
                                      return;
                                    }

                                    impl_->active_config = loader.load();
                                    if (impl_->bridge.is_running()) {
                                      impl_->bridge.reload(impl_->active_config);
                                    } else {
                                      impl_->bridge.start(impl_->active_config);
                                    }
                                    apply_routing(impl_->bridge, impl_->routing);
                                    emit_snapshot(event);
                                  });

  const auto started = impl_->bridge.start(impl_->active_config);
  if (!started.ok) {
    logger.log(homepi::logging::LogLevel::WARN, "pcm.router", "audio_bridge_degraded", "startup",
               started.error);
  } else {
    events.emit_service_status("service_ready", "startup", "healthy");
  }
  events.emit_service_status("service_started", "startup", started.ok ? "healthy" : "degraded");
  emit_snapshot("startup");

  while (!g_stop.load()) {
    const auto is_zone_ready = [this](int zone_id) {
      return impl_->bridge.zone_available_frames(zone_id) >= config_.period_frames;
    };
    if (impl_->routing.try_promote_pending_owner(is_zone_ready)) {
      apply_routing(impl_->bridge, impl_->routing);
      emit_snapshot("owner_promoted");
    }
    const int sleep_ms = impl_->routing.pending_owner_zone_id() > 0 ? 10 : 500;
    std::this_thread::sleep_for(std::chrono::milliseconds(sleep_ms));
  }

  shutdown();
  events.emit_service_status("service_stopped", "shutdown", "offline");
  return 0;
}

void Service::shutdown() {
  impl_->profile_subscriber.stop();
  if (impl_->bridge.is_running()) {
    impl_->bridge.stop();
  }
  if (impl_->server) {
    impl_->server->stop();
  }
}

int Service::validate_and_exit(const ServiceConfig& /*config*/, const std::string& /*mode*/) {
  return 0;
}

}  // namespace homepi::pcm_router
