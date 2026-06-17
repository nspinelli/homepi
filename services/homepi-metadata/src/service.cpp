#include "homepi/metadata/service.hpp"

#include <atomic>
#include <chrono>
#include <csignal>
#include <memory>
#include <thread>
#include <utility>
#include <vector>

#include "homepi/events/event-emitter.hpp"
#include "homepi/events/event-envelope.hpp"
#include "homepi/log.hpp"
#include "homepi/metadata/metadata-state-repository.hpp"
#include "homepi/metadata/mqtt-subscriber.hpp"
#include "homepi/metadata/now-playing-state.hpp"
#include "homepi/metadata/pcm-router-subscriber.hpp"
#include "homepi/metadata/pipe-manager.hpp"
#include "homepi/metadata/snapshot-builder.hpp"
#include "homepi/metadata/unix-api-server.hpp"

namespace homepi::metadata {

namespace {

std::atomic<bool> g_stop{false};

void on_signal(int) { g_stop = true; }

}  // namespace

struct Service::Impl {
  NowPlayingState state;
  std::unique_ptr<MetadataStateRepository> repository;
  std::unique_ptr<PipeManager> pipes;
  std::unique_ptr<MqttSubscriber> mqtt_subscriber;
  std::unique_ptr<PcmRouterSubscriber> pcm_subscriber;
  std::unique_ptr<UnixApiServer> server;
  homepi::events::EventEmitter* events = nullptr;
};

Service::Service(ServiceConfig config) : config_(std::move(config)), impl_(std::make_unique<Impl>()) {}

Service::~Service() { shutdown(); }

int Service::run() {
  std::signal(SIGINT, on_signal);
  std::signal(SIGTERM, on_signal);

  const auto log_level = config_.log_level == "DEBUG"   ? homepi::logging::LogLevel::DEBUG
                         : config_.log_level == "WARN" ? homepi::logging::LogLevel::WARN
                         : config_.log_level == "ERROR"
                             ? homepi::logging::LogLevel::ERROR
                             : homepi::logging::LogLevel::INFO;
  homepi::logging::Logger logger(config_.service, log_level);

  impl_->repository = std::make_unique<MetadataStateRepository>(config_.database_path);

  impl_->server = std::make_unique<UnixApiServer>(
      config_.socket_path,
      [this](const std::string& correlation_id) {
        homepi::events::EventEnvelope envelope;
        envelope.id = config_.service + "-snapshot";
        envelope.source = config_.service;
        envelope.topic = "modules.metadata.snapshot";
        envelope.event = "metadata_snapshot";
        envelope.correlation_id = correlation_id;
        envelope.timestamp = homepi::events::iso_timestamp();
        envelope.payload_json = SnapshotBuilder::build_payload(impl_->state.snapshot());
        return homepi::events::build_event_line(envelope);
      });

  homepi::events::EventEmitter events(config_.service, [this](const std::string& line) {
    if (impl_->server) {
      impl_->server->broadcast(line);
    }
  });
  impl_->events = &events;

  const auto emit_snapshot = [&](const std::string& correlation_id) {
    events.emit("modules.metadata.snapshot", "metadata_snapshot", correlation_id,
                SnapshotBuilder::build_payload(impl_->state.snapshot()));
  };

  const auto persist_and_emit_progress = [&](int zone_id, int position_ms, int duration_ms,
                                             bool playing) {
    if (!impl_->state.update_progress(zone_id, position_ms, duration_ms, playing)) {
      return;
    }
    const auto snapshot = impl_->state.snapshot();
    if (snapshot.duration_ms > 0 && !snapshot.track_id.empty()) {
      impl_->repository->cache_track_duration(snapshot.track_id, snapshot.duration_ms);
    }
    impl_->repository->save_snapshot(snapshot);
    events.emit("modules.metadata.progress", "metadata_progress_updated", "progress",
                SnapshotBuilder::build_progress_payload(
                    zone_id,
                    snapshot.position_ms,
                    snapshot.duration_ms,
                    playing));
  };

  const auto apply_cached_duration = [&](int zone_id) {
    const auto snapshot = impl_->state.snapshot();
    if (snapshot.duration_ms > 0 || snapshot.track_id.empty()) {
      return;
    }
    if (const auto cached = impl_->repository->load_cached_track_duration(snapshot.track_id)) {
      persist_and_emit_progress(zone_id, -1, *cached, snapshot.playing);
    }
  };

  const auto persist_and_emit_field = [&](int zone_id, const std::string& field,
                                          const std::string& value) {
    std::string resolvedField = field;
    if (field == "client_model") {
      if (!impl_->state.snapshot().client_name.empty()) {
        return;
      }
      resolvedField = "client_name";
    }
    if (!impl_->state.update_field(zone_id, resolvedField, value)) {
      return;
    }
    const auto snapshot = impl_->state.snapshot();
    impl_->repository->save_snapshot(snapshot);
    events.emit("modules.metadata.now_playing", "metadata_field_updated", resolvedField,
                SnapshotBuilder::build_field_payload(zone_id, resolvedField, value));
    if (resolvedField == "track_id") {
      apply_cached_duration(zone_id);
    }
  };

  const auto persist_and_emit_playing = [&](int zone_id, bool playing) {
    if (!impl_->state.update_playing(zone_id, playing)) {
      return;
    }
    const auto snapshot = impl_->state.snapshot();
    impl_->repository->save_snapshot(snapshot);
    events.emit("modules.metadata.progress", "playback_state_changed", "playback",
                SnapshotBuilder::build_progress_payload(zone_id, snapshot.position_ms,
                                                        snapshot.duration_ms, playing));
  };

  const auto handle_owner_change = [&](int owner_zone_id) {
    const bool changed = impl_->state.set_owner_zone(owner_zone_id);
    impl_->pipes->set_owner_zone(owner_zone_id);
    if (impl_->mqtt_subscriber) {
      impl_->mqtt_subscriber->set_owner_zone(owner_zone_id);
    }
    if (!changed) {
      return;
    }
    if (owner_zone_id <= 0) {
      const int previous_owner = impl_->state.snapshot().owner_zone_id;
      impl_->state.set_owner_zone(0);
      if (previous_owner > 0) {
        impl_->repository->clear_owner(previous_owner);
      }
      events.emit("modules.metadata.now_playing", "metadata_cleared", "owner-cleared", "{}");
      emit_snapshot("owner-cleared");
      return;
    }

    impl_->state.set_owner_zone(owner_zone_id);
    if (const auto restored = impl_->repository->load_owner(owner_zone_id)) {
      for (const auto& field :
           {std::pair{"title", restored->title}, std::pair{"artist", restored->artist},
            std::pair{"album", restored->album}}) {
        if (!field.second.empty()) {
          impl_->state.update_field(owner_zone_id, field.first, field.second);
        }
      }
      impl_->state.update_progress(owner_zone_id, restored->position_ms, restored->duration_ms,
                                  restored->playing);
      if (!restored->track_id.empty()) {
        impl_->state.update_field(owner_zone_id, "track_id", restored->track_id);
      }
      if (restored->has_cover_art) {
        impl_->state.mark_cover_art(owner_zone_id);
      }
      apply_cached_duration(owner_zone_id);
    }
    emit_snapshot("owner-changed");
  };

  impl_->pipes = std::make_unique<PipeManager>();
  PipeManagerCallbacks pipe_callbacks;
  pipe_callbacks.on_field = [&](int zone_id, const std::string& field, const std::string& value) {
    persist_and_emit_field(zone_id, field, value);
  };
  pipe_callbacks.on_progress = [&](int zone_id, int position_ms, int duration_ms, bool playing) {
    persist_and_emit_progress(zone_id, position_ms, duration_ms, playing);
  };
  pipe_callbacks.on_playback_state = persist_and_emit_playing;
  pipe_callbacks.on_cover_art = [&](int zone_id, const std::vector<std::uint8_t>& bytes) {
    if (!impl_->state.mark_cover_art(zone_id, true)) {
      return;
    }
    MetadataStateRepository::write_cover_art(config_.cache_dir, zone_id, bytes);
    impl_->repository->save_snapshot(impl_->state.snapshot());
    events.emit("modules.metadata.cover_art", "metadata_cover_updated", "cover",
                SnapshotBuilder::build_cover_payload(zone_id));
  };
  pipe_callbacks.on_metadata_bundle_start = [&](int zone_id) {
    if (zone_id != impl_->state.snapshot().owner_zone_id) {
      return;
    }
    impl_->state.clear_metadata_fields();
    MetadataStateRepository::delete_cover_art(config_.cache_dir, zone_id);
    impl_->repository->save_snapshot(impl_->state.snapshot());
    emit_snapshot("metadata-bundle-start");
  };
  pipe_callbacks.on_session_cleared = [&](int zone_id) {
    if (zone_id != impl_->state.snapshot().owner_zone_id) {
      return;
    }
    const auto snapshot = impl_->state.snapshot();
    if (snapshot.duration_ms <= 0 && snapshot.position_ms > 0) {
      persist_and_emit_progress(zone_id, snapshot.position_ms, snapshot.position_ms, false);
      if (!snapshot.track_id.empty()) {
        impl_->repository->cache_track_duration(snapshot.track_id, snapshot.position_ms);
      }
    }
    impl_->state.clear();
    MetadataStateRepository::delete_cover_art(config_.cache_dir, zone_id);
    impl_->repository->clear_owner(zone_id);
    events.emit("modules.metadata.now_playing", "metadata_cleared", "session-cleared", "{}");
    emit_snapshot("session-cleared");
  };
  impl_->pipes->start(config_.pipe_prefix, config_.zone_count, std::move(pipe_callbacks));

  MqttSubscriberCallbacks mqtt_callbacks;
  mqtt_callbacks.on_field = persist_and_emit_field;
  mqtt_callbacks.on_progress = persist_and_emit_progress;
  mqtt_callbacks.on_playback_state = persist_and_emit_playing;
  mqtt_callbacks.on_cover_art = [this, &events](int zone_id, const std::vector<std::uint8_t>& bytes) {
    if (!impl_->state.mark_cover_art(zone_id, true)) {
      return;
    }
    MetadataStateRepository::write_cover_art(config_.cache_dir, zone_id, bytes);
    impl_->repository->save_snapshot(impl_->state.snapshot());
    events.emit("modules.metadata.cover_art", "metadata_cover_updated", "cover",
                SnapshotBuilder::build_cover_payload(zone_id));
  };
  mqtt_callbacks.on_metadata_bundle_start = [this, &emit_snapshot](int zone_id) {
    if (zone_id != impl_->state.snapshot().owner_zone_id) {
      return;
    }
    impl_->state.clear_metadata_fields();
    MetadataStateRepository::delete_cover_art(config_.cache_dir, zone_id);
    impl_->repository->save_snapshot(impl_->state.snapshot());
    emit_snapshot("metadata-bundle-start");
  };
  mqtt_callbacks.on_session_cleared = [this, &events, &emit_snapshot, &persist_and_emit_progress](
                                          int zone_id) {
    if (zone_id != impl_->state.snapshot().owner_zone_id) {
      return;
    }
    const auto snapshot = impl_->state.snapshot();
    if (snapshot.duration_ms <= 0 && snapshot.position_ms > 0) {
      persist_and_emit_progress(zone_id, snapshot.position_ms, snapshot.position_ms, false);
      if (!snapshot.track_id.empty()) {
        impl_->repository->cache_track_duration(snapshot.track_id, snapshot.position_ms);
      }
    }
    impl_->state.clear();
    MetadataStateRepository::delete_cover_art(config_.cache_dir, zone_id);
    impl_->repository->clear_owner(zone_id);
    events.emit("modules.metadata.now_playing", "metadata_cleared", "session-cleared", "{}");
    emit_snapshot("session-cleared");
  };

  impl_->mqtt_subscriber = std::make_unique<MqttSubscriber>();
  if (!impl_->mqtt_subscriber->start(config_.mqtt_host, config_.mqtt_port, config_.mqtt_topic_prefix,
                                     std::move(mqtt_callbacks))) {
    logger.log(homepi::logging::LogLevel::ERROR, "metadata.runtime", "mqtt_connect_failed",
               "startup", "failed to connect metadata MQTT broker");
    return 1;
  }

  impl_->pcm_subscriber = std::make_unique<PcmRouterSubscriber>();
  impl_->pcm_subscriber->start(config_.pcm_router_socket, handle_owner_change);
  handle_owner_change(impl_->pcm_subscriber->owner_zone_id());

  if (!impl_->server->start()) {
    logger.log(homepi::logging::LogLevel::ERROR, "metadata.runtime", "socket_start_failed",
               "startup", "failed to bind metadata unix socket");
    return 1;
  }

  events.emit_service_status("service_started", "startup", "healthy");
  events.emit_service_status("service_ready", "startup", "healthy");
  emit_snapshot("startup");

  while (!g_stop.load()) {
    std::this_thread::sleep_for(std::chrono::milliseconds(250));
  }

  shutdown();
  events.emit_service_status("service_stopped", "shutdown", "offline");
  return 0;
}

void Service::shutdown() {
  if (impl_->pcm_subscriber) {
    impl_->pcm_subscriber->stop();
  }
  if (impl_->mqtt_subscriber) {
    impl_->mqtt_subscriber->stop();
  }
  if (impl_->pipes) {
    impl_->pipes->stop();
  }
  if (impl_->server) {
    impl_->server->stop();
  }
}

}  // namespace homepi::metadata
