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
#include "homepi/metadata/events-owner-subscriber.hpp"
#include "homepi/metadata/metadata-coalescer.hpp"
#include "homepi/metadata/metadata-state-repository.hpp"
#include "homepi/metadata/now-playing-state.hpp"
#include "homepi/metadata/pipe-manager.hpp"
#include "homepi/metadata/realtime-progress-server.hpp"
#include "homepi/metadata/snapshot-builder.hpp"
#include "homepi/metadata/unix-api-server.hpp"

namespace homepi::metadata {

namespace {

std::atomic<bool> g_stop{false};

void on_signal(int) { g_stop = true; }

bool snapshot_has_content(const NowPlayingSnapshot& snapshot) {
  return !snapshot.title.empty() || !snapshot.artist.empty() || !snapshot.album.empty();
}

}  // namespace

struct Service::Impl {
  NowPlayingState state;
  std::unique_ptr<MetadataStateRepository> repository;
  std::unique_ptr<PipeManager> pipes;
  std::unique_ptr<EventsOwnerSubscriber> events_subscriber;
  std::unique_ptr<MetadataCoalescer> coalescer;
  std::unique_ptr<RealtimeProgressServer> realtime_server;
  std::unique_ptr<UnixApiServer> server;
  homepi::events::EventEmitter* events = nullptr;
  std::chrono::steady_clock::time_point last_progress_persist_{};
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

  const auto emit_snapshot = [&](const std::string& correlation_id, bool allow_empty = false) {
    const auto snapshot = impl_->state.snapshot();
    if (!allow_empty && !snapshot_has_content(snapshot)) {
      return;
    }
    const std::string payload = SnapshotBuilder::build_payload(snapshot);
    events.emit("modules.metadata.snapshot", "metadata_snapshot", correlation_id, payload);
    events.emit("modules.metadata.now_playing", "metadata_track_changed", correlation_id, payload);
  };

  impl_->coalescer = std::make_unique<MetadataCoalescer>(
      config_.metadata_debounce_ms,
      [this, &emit_snapshot](const std::string& reason) { emit_snapshot(reason); });

  impl_->realtime_server = std::make_unique<RealtimeProgressServer>();

  const auto publish_realtime_progress = [&](const std::string& source) {
    const auto snapshot = impl_->state.snapshot();
    if (snapshot.owner_zone_id <= 0) {
      return;
    }
    impl_->realtime_server->publish_progress(snapshot.owner_zone_id, snapshot.track_id,
                                            snapshot.playing, snapshot.position_ms,
                                            snapshot.duration_ms, source);
  };

  const auto maybe_persist_progress = [&](bool force) {
    const auto now = std::chrono::steady_clock::now();
    if (!force &&
        now - impl_->last_progress_persist_ < std::chrono::seconds(5)) {
      return;
    }
    impl_->last_progress_persist_ = now;
    impl_->repository->save_snapshot(impl_->state.snapshot());
  };

  const auto persist_and_publish_progress = [&](int zone_id, int position_ms, int duration_ms,
                                                bool playing, const std::string& source) {
    if (!impl_->state.update_progress(zone_id, position_ms, duration_ms, playing)) {
      return;
    }
    const auto snapshot = impl_->state.snapshot();
    if (snapshot.duration_ms > 0 && !snapshot.track_id.empty()) {
      impl_->repository->cache_track_duration(snapshot.track_id, snapshot.duration_ms);
    }
    publish_realtime_progress(source);
    maybe_persist_progress(false);
    if (snapshot.duration_ms > 0) {
      impl_->coalescer->schedule_flush("duration-updated");
    }
  };

  const auto apply_cached_duration = [&](int zone_id) {
    const auto snapshot = impl_->state.snapshot();
    if (snapshot.duration_ms > 0 || snapshot.track_id.empty()) {
      return;
    }
    if (const auto cached = impl_->repository->load_cached_track_duration(snapshot.track_id)) {
      persist_and_publish_progress(zone_id, -1, *cached, snapshot.playing, "cache:track_duration");
    }
  };

  const auto apply_field_update = [&](int zone_id, const std::string& field,
                                      const std::string& value) {
    std::string resolved_field = field;
    if (field == "client_model") {
      if (!impl_->state.snapshot().client_name.empty()) {
        return;
      }
      resolved_field = "client_name";
    }
    if (!impl_->state.update_field(zone_id, resolved_field, value)) {
      return;
    }
    if (resolved_field == "track_id") {
      apply_cached_duration(zone_id);
    }
    if (resolved_field == "title" && !value.empty() && !impl_->coalescer->awaiting_bundle_end()) {
      impl_->coalescer->schedule_flush("title-updated", true);
      return;
    }
    if (!impl_->coalescer->awaiting_bundle_end()) {
      impl_->coalescer->schedule_flush("field-updated");
    }
  };

  const auto persist_and_emit_playing = [&](int zone_id, bool playing) {
    if (!impl_->state.update_playing(zone_id, playing)) {
      return;
    }
    publish_realtime_progress("pipe:playback_state");
    maybe_persist_progress(true);
    events.emit("modules.metadata.progress", "playback_state_changed", "playback",
                SnapshotBuilder::build_progress_payload(zone_id, impl_->state.snapshot().position_ms,
                                                        impl_->state.snapshot().duration_ms,
                                                        playing));
  };

  const auto handle_owner_change = [&](int owner_zone_id) {
    const int previous_owner = impl_->state.snapshot().owner_zone_id;
    const bool changed = impl_->state.set_owner_zone(owner_zone_id);
    impl_->pipes->set_owner_zone(owner_zone_id);
    if (!changed) {
      return;
    }
    if (owner_zone_id <= 0) {
      if (previous_owner > 0) {
        impl_->repository->clear_owner(previous_owner);
      }
      events.emit("modules.metadata.now_playing", "metadata_cleared", "owner-cleared", "{}");
      emit_snapshot("owner-cleared", true);
      return;
    }

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
  pipe_callbacks.on_field = apply_field_update;
  pipe_callbacks.on_progress = [&](int zone_id, int position_ms, int duration_ms, bool playing) {
    persist_and_publish_progress(zone_id, position_ms, duration_ms, playing, "pipe:ssnc/prgr");
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
    impl_->coalescer->schedule_flush("cover-updated", true);
  };
  pipe_callbacks.on_metadata_bundle_start = [&](int zone_id) {
    if (zone_id != impl_->state.snapshot().owner_zone_id) {
      return;
    }
    impl_->coalescer->set_awaiting_bundle_end(true);
    impl_->coalescer->cancel_pending();
    impl_->state.begin_metadata_bundle();
    impl_->repository->save_snapshot(impl_->state.snapshot());
  };
  pipe_callbacks.on_metadata_bundle_end = [&](int zone_id) {
    if (zone_id != impl_->state.snapshot().owner_zone_id) {
      return;
    }
    impl_->coalescer->set_awaiting_bundle_end(false);
    const bool has_title = !impl_->state.snapshot().title.empty();
    impl_->coalescer->schedule_flush("bundle-end", has_title);
    impl_->repository->save_snapshot(impl_->state.snapshot());
  };
  pipe_callbacks.on_session_cleared = [&](int zone_id) {
    if (zone_id != impl_->state.snapshot().owner_zone_id) {
      return;
    }
    const auto snapshot = impl_->state.snapshot();
    if (snapshot.duration_ms <= 0 && snapshot.position_ms > 0) {
      persist_and_publish_progress(zone_id, snapshot.position_ms, snapshot.position_ms, false,
                                   "pipe:session_end");
      if (!snapshot.track_id.empty()) {
        impl_->repository->cache_track_duration(snapshot.track_id, snapshot.position_ms);
      }
    }
    impl_->repository->record_play_history(impl_->state.snapshot());
    impl_->state.clear();
    MetadataStateRepository::delete_cover_art(config_.cache_dir, zone_id);
    impl_->repository->clear_owner(zone_id);
    events.emit("modules.metadata.now_playing", "metadata_cleared", "session-cleared", "{}");
    emit_snapshot("session-cleared", true);
    publish_realtime_progress("pipe:session_cleared");
  };
  impl_->pipes->start(config_.pipe_prefix, config_.zone_count, std::move(pipe_callbacks));

  impl_->events_subscriber = std::make_unique<EventsOwnerSubscriber>();
  impl_->events_subscriber->start(
      config_.events_socket, config_.service, handle_owner_change,
      [this](const std::vector<int>& enabled_zone_ids) {
        impl_->pipes->set_enabled_zones(enabled_zone_ids);
      });
  handle_owner_change(impl_->events_subscriber->owner_zone_id());

  if (!impl_->realtime_server->start(config_.realtime_socket_path)) {
    logger.log(homepi::logging::LogLevel::ERROR, "metadata.runtime", "realtime_socket_failed",
               "startup", "failed to bind audio-realtime socket");
    return 1;
  }

  if (!impl_->server->start()) {
    logger.log(homepi::logging::LogLevel::ERROR, "metadata.runtime", "socket_start_failed",
               "startup", "failed to bind metadata unix socket");
    return 1;
  }

  events.emit_service_status("service_started", "startup", "healthy");
  events.emit_service_status("service_ready", "startup", "healthy");
  emit_snapshot("startup", true);

  while (!g_stop.load()) {
    impl_->coalescer->tick();
    std::this_thread::sleep_for(std::chrono::milliseconds(50));
  }

  shutdown();
  events.emit_service_status("service_stopped", "shutdown", "offline");
  return 0;
}

void Service::shutdown() {
  if (impl_->events_subscriber) {
    impl_->events_subscriber->stop();
  }
  if (impl_->realtime_server) {
    impl_->realtime_server->stop();
  }
  if (impl_->pipes) {
    impl_->pipes->stop();
  }
  if (impl_->server) {
    impl_->server->stop();
  }
}

}  // namespace homepi::metadata
