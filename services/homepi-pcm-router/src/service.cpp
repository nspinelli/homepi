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
#include "homepi/events/events-client.hpp"
#include "homepi/log.hpp"
#include "homepi/pcm-router/audio-bridge.hpp"
#include "homepi/pcm-router/audio-profile-loader.hpp"
#include "homepi/pcm-router/event-subscriber.hpp"
#include "homepi/pcm-router/json-utils.hpp"
#include "homepi/pcm-router/routing-state.hpp"
#include "homepi/pcm-router/snapshot-builder.hpp"
#include "homepi/pcm-router/unix-api-server.hpp"
#include "homepi/pcm-router/zone-enable-loader.hpp"

namespace homepi::pcm_router {

namespace {

std::atomic<bool> g_stop{false};

void on_signal(int) { g_stop = true; }

void apply_routing(AudioBridge& bridge, RoutingState& routing) {
  bridge.apply_zone_modes(routing.zone_modes());
  const int playback_zone = routing.handoff_owner_zone_id() > 0 ? routing.handoff_owner_zone_id()
                                                              : routing.owner_zone_id();
  if (playback_zone > 0) {
    bridge.set_playback_owner(playback_zone);
  }
}

std::string stack_json(const std::vector<int>& stack) {
  std::ostringstream out;
  out << '[';
  for (size_t i = 0; i < stack.size(); ++i) {
    if (i > 0) {
      out << ',';
    }
    out << stack[i];
  }
  out << ']';
  return out.str();
}

void emit_owner_routing_events(homepi::events::EventEmitter& events, const RoutingState& routing,
                               const std::string& correlation_id, int previous_owner,
                               int previous_pending) {
  const int owner = routing.owner_zone_id();
  const int pending = routing.pending_owner_zone_id();
  if (pending > 0 && pending != previous_pending) {
    std::ostringstream payload;
    payload << "{\"ownerZoneId\":" << owner << ",\"pendingOwnerZoneId\":" << pending
            << ",\"activeStack\":" << stack_json(routing.active_stack()) << "}";
    events.emit("modules.pcm.routing", "owner_pending", correlation_id, payload.str());
  }
  if (owner != previous_owner) {
    std::ostringstream payload;
    payload << "{\"ownerZoneId\":" << owner << ",\"pendingOwnerZoneId\":" << pending
            << ",\"activeStack\":" << stack_json(routing.active_stack()) << "}";
    events.emit("modules.pcm.routing", "owner_changed", correlation_id, payload.str());
  }
}

void emit_routing_changed(homepi::events::EventEmitter& events, const std::string& snapshot_json,
                          const std::string& correlation_id, int zone_id,
                          const std::string& method) {
  std::ostringstream routing_payload;
  routing_payload << snapshot_json.substr(0, snapshot_json.size() - 1) << ",\"zoneId\":" << zone_id
                  << ",\"method\":\"" << method << "\"}";
  events.emit("modules.pcm", "routing_changed", correlation_id, routing_payload.str());
}

void emit_zone_capture_closed(homepi::events::EventEmitter& events, int zone_id,
                              const std::string& reason, const std::string& correlation_id) {
  std::ostringstream payload;
  payload << "{\"zoneId\":" << zone_id << ",\"captureOpen\":false,\"reason\":\"" << reason
          << "\"}";
  events.emit("modules.pcm.routing", "zone_capture_closed", correlation_id, payload.str());
}

}  // namespace

struct Service::Impl {
  RoutingState routing;
  AudioBridge bridge;
  ActiveAudioConfig active_config;
  std::unique_ptr<UnixApiServer> server;
  EventSubscriber profile_subscriber;
  std::unique_ptr<homepi::events::EventsClient> events_client;
  homepi::events::EventEmitter* events = nullptr;

  explicit Impl(ServiceConfig config) : bridge(config) {}
};

Service::Service(ServiceConfig config)
    : config_(std::move(config)), impl_(std::make_unique<Impl>(config_)) {}

Service::~Service() = default;

std::string Service::build_snapshot_json() const {
  return SnapshotBuilder::build_payload(impl_->routing, impl_->bridge, impl_->active_config,
                                        config_.zone_count);
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
  impl_->routing.load_enabled_mask(
      load_enabled_zone_mask(config_.database_path, config_.zone_count));

  homepi::events::EventEmitter events(config_.service, [&](const std::string& line) {
    if (impl_->server) {
      impl_->server->broadcast(line);
    }
    if (impl_->events_client) {
      impl_->events_client->publish(line);
    }
  });
  impl_->events = &events;

  const auto emit_snapshot = [&](const std::string& correlation_id) {
    events.emit("modules.pcm.snapshot", "pcm_router_snapshot", correlation_id,
                build_snapshot_json());
  };

  const auto apply_set_zone_enabled = [&](int zone_id, bool enabled,
                                          const std::string& correlation_id) {
    const SetZoneEnabledResult result = impl_->routing.set_zone_enabled(zone_id, enabled);
    if (!result.changed) {
      return;
    }
    apply_routing(impl_->bridge, impl_->routing);
    if (!enabled) {
      emit_zone_capture_closed(events, zone_id, "zone_disabled", correlation_id);
    }
    emit_snapshot(correlation_id.empty() ? "zone_enabled_changed" : correlation_id);
    if (!enabled) {
      std::ostringstream disabled_payload;
      disabled_payload << "{\"zoneId\":" << zone_id << "}";
      events.emit("modules.pcm.routing", "zone_disabled", correlation_id,
                  disabled_payload.str());
      if (result.owner_changed) {
        std::ostringstream owner_payload;
        owner_payload << "{\"ownerZoneId\":" << result.new_owner_zone_id
                      << ",\"previousOwnerZoneId\":" << result.previous_owner_zone_id
                      << ",\"disabledZoneId\":" << zone_id << ",\"activeStack\":"
                      << stack_json(impl_->routing.active_stack())
                      << ",\"reason\":\"owner_disabled\"}";
        events.emit("modules.pcm.routing", "owner_changed", correlation_id,
                    owner_payload.str());
      }
    }
  };

  const auto handle_prewarm_capture = [&](int zone_id, const std::string& correlation_id) {
    if (zone_id < 1 || zone_id > config_.zone_count || !impl_->routing.is_zone_enabled(zone_id)) {
      return;
    }
    impl_->bridge.prewarm_zone_capture(zone_id);
    emit_snapshot(correlation_id.empty() ? "prewarm_capture" : correlation_id);
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
      [this, &emit_snapshot, &events, &apply_set_zone_enabled, &handle_prewarm_capture](
          const std::string& method, int /*zone_id*/, const std::string& correlation_id,
          const std::string& body_json) {
        const int zone_id = parse_int_field(body_json, "zoneId");
        const int previous_owner = impl_->routing.owner_zone_id();
        const int previous_pending = impl_->routing.pending_owner_zone_id();
        if (method == "route_start") {
          const auto is_zone_active = [this](int stacked_zone_id) {
            return impl_->bridge.zone_recently_buffered(stacked_zone_id, kLivePcmActiveMs);
          };
          impl_->routing.on_route_start(zone_id, is_zone_active, impl_->bridge.playback_owner());
          if (impl_->routing.handoff_owner_zone_id() == 0 &&
              impl_->routing.pending_owner_zone_id() == zone_id) {
            impl_->bridge.clear_zone_ring(zone_id);
          }
        } else if (method == "route_end") {
          impl_->routing.on_route_end(zone_id);
          apply_routing(impl_->bridge, impl_->routing);
          impl_->bridge.schedule_zone_capture_idle_close(zone_id);
          emit_owner_routing_events(events, impl_->routing, correlation_id, previous_owner,
                                    previous_pending);
          const std::string snapshot = build_snapshot_json();
          emit_snapshot(correlation_id.empty() ? "routing_changed" : correlation_id);
          emit_routing_changed(events, snapshot,
                               correlation_id.empty() ? "routing_changed" : correlation_id,
                               zone_id, method);
          return;
        } else if (method == "route_join") {
          impl_->routing.on_route_join(zone_id);
          handle_prewarm_capture(zone_id, correlation_id);
        } else if (method == "set_routing") {
          impl_->routing.set_routing(parse_int_field(body_json, "ownerZoneId"),
                                     parse_int_array(body_json, "activeStack"));
        } else if (method == "set_zone_enabled") {
          const bool enabled = parse_bool_field(body_json, "enabled");
          apply_set_zone_enabled(zone_id, enabled, correlation_id);
          return;
        } else if (method == "prewarm_capture") {
          handle_prewarm_capture(zone_id, correlation_id);
          return;
        }
        apply_routing(impl_->bridge, impl_->routing);
        emit_owner_routing_events(events, impl_->routing, correlation_id, previous_owner,
                                  previous_pending);
        const std::string snapshot = build_snapshot_json();
        emit_snapshot(correlation_id.empty() ? "routing_changed" : correlation_id);
        emit_routing_changed(events, snapshot,
                             correlation_id.empty() ? "routing_changed" : correlation_id, zone_id,
                             method);
      });

  if (!impl_->server->start()) {
    logger.log(homepi::logging::LogLevel::ERROR, "pcm.router", "socket_start_failed", "startup",
               "failed to bind unix socket");
    return 1;
  }

  impl_->events_client = std::make_unique<homepi::events::EventsClient>(config_.events_socket,
                                                                        config_.service);
  impl_->events_client->start(
      {"modules.pcm.command", "modules.zone.config"},
      {"modules.pcm.snapshot", "modules.pcm.routing", "modules.pcm", "modules.service.status"},
      [this, &apply_set_zone_enabled, &handle_prewarm_capture, &events, &emit_snapshot](
          const std::string& line) {
        const std::string event = parse_event_name(line);
        std::string correlation_id = "broker";
        const std::string key = "\"correlationId\"";
        const auto pos = line.find(key);
        if (pos != std::string::npos) {
          const auto quote_start = line.find('"', pos + key.size());
          if (quote_start != std::string::npos) {
            const auto quote_end = line.find('"', quote_start + 1);
            if (quote_end != std::string::npos) {
              correlation_id = line.substr(quote_start + 1, quote_end - quote_start - 1);
            }
          }
        }
        const std::string payload = parse_payload_json(line);
        const int zone_id = parse_int_field(payload, "zoneId");
        if (event == "set_zone_enabled" || event == "zone_enabled_changed") {
          apply_set_zone_enabled(zone_id, parse_bool_field(payload, "enabled"), correlation_id);
          return;
        }
        if (event == "prewarm_capture") {
          handle_prewarm_capture(zone_id, correlation_id);
          return;
        }
        if (event == "route_start") {
          const int previous_owner = impl_->routing.owner_zone_id();
          const int previous_pending = impl_->routing.pending_owner_zone_id();
          const auto is_zone_active = [this](int stacked_zone_id) {
            return impl_->bridge.zone_recently_buffered(stacked_zone_id, kLivePcmActiveMs);
          };
          impl_->routing.on_route_start(zone_id, is_zone_active, impl_->bridge.playback_owner());
          if (impl_->routing.handoff_owner_zone_id() == 0 &&
              impl_->routing.pending_owner_zone_id() == zone_id) {
            impl_->bridge.clear_zone_ring(zone_id);
          }
          apply_routing(impl_->bridge, impl_->routing);
          emit_owner_routing_events(events, impl_->routing, correlation_id, previous_owner,
                                    previous_pending);
          emit_snapshot(correlation_id);
          return;
        }
        if (event == "route_end") {
          const int previous_owner = impl_->routing.owner_zone_id();
          const int previous_pending = impl_->routing.pending_owner_zone_id();
          impl_->routing.on_route_end(zone_id);
          apply_routing(impl_->bridge, impl_->routing);
          impl_->bridge.schedule_zone_capture_idle_close(zone_id);
          emit_owner_routing_events(events, impl_->routing, correlation_id, previous_owner,
                                    previous_pending);
          const std::string snapshot = build_snapshot_json();
          emit_snapshot(correlation_id);
          emit_routing_changed(events, snapshot, correlation_id, zone_id, "route_end");
          return;
        }
        if (event == "route_join") {
          impl_->routing.on_route_join(zone_id);
          handle_prewarm_capture(zone_id, correlation_id);
          apply_routing(impl_->bridge, impl_->routing);
          emit_snapshot(correlation_id);
        }
      });

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

  int64_t last_lifecycle_tick_ms = 0;
  while (!g_stop.load()) {
    const auto is_zone_ready = [this](int zone_id) {
      if (impl_->bridge.zone_available_frames(zone_id) < config_.period_frames) {
        return false;
      }
      if (impl_->routing.handoff_owner_zone_id() > 0) {
        return true;
      }
      return impl_->bridge.zone_recently_buffered(zone_id, kHandoffFreshBufferMs);
    };
    const int previous_owner = impl_->routing.owner_zone_id();
    const int previous_pending = impl_->routing.pending_owner_zone_id();
    if (impl_->routing.try_promote_pending_owner(is_zone_ready)) {
      apply_routing(impl_->bridge, impl_->routing);
      emit_owner_routing_events(events, impl_->routing, "owner_promoted", previous_owner,
                                previous_pending);
      emit_snapshot("owner_promoted");
    }

    const int64_t now_ms = std::chrono::duration_cast<std::chrono::milliseconds>(
                               std::chrono::steady_clock::now().time_since_epoch())
                               .count();
    if (now_ms - last_lifecycle_tick_ms >= 100) {
      last_lifecycle_tick_ms = now_ms;
      const CaptureLifecycleTick tick = impl_->bridge.tick_capture_lifecycle();
      if (!tick.closed_zones.empty()) {
        for (int zone_id : tick.closed_zones) {
          emit_zone_capture_closed(events, zone_id, "idle_grace_expired", "capture_lifecycle");
        }
        emit_snapshot("capture_lifecycle");
      }
    }

    const int sleep_ms =
        impl_->routing.pending_owner_zone_id() > 0 || impl_->routing.handoff_owner_zone_id() > 0
            ? 1
            : 100;
    std::this_thread::sleep_for(std::chrono::milliseconds(sleep_ms));
  }

  shutdown();
  events.emit_service_status("service_stopped", "shutdown", "offline");
  return 0;
}

void Service::shutdown() {
  if (impl_->events_client) {
    impl_->events_client->stop();
    impl_->events_client.reset();
  }
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
