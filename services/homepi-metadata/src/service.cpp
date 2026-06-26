#include "homepi/metadata/service.hpp"

#include <atomic>
#include <chrono>
#include <csignal>
#include <algorithm>
#include <memory>
#include <thread>
#include <utility>
#include <vector>

#include "homepi/events/event-emitter.hpp"
#include "homepi/events/event-envelope.hpp"
#include "homepi/events/events-client.hpp"
#include "homepi/log.hpp"
#include "homepi/metadata/cover-art-validator.hpp"
#include "homepi/metadata/events-owner-subscriber.hpp"
#include "homepi/metadata/metadata-coalescer.hpp"
#include "homepi/metadata/metadata-state-repository.hpp"
#include "homepi/metadata/now-playing-state.hpp"
#include "homepi/metadata/pipe-manager.hpp"
#include "homepi/metadata/json-utils.hpp"
#include "homepi/metadata/metadata-normalizer.hpp"
#include "homepi/metadata/realtime-progress-server.hpp"
#include "homepi/metadata/service-event-loop.hpp"
#include "homepi/metadata/snapshot-builder.hpp"
#include "homepi/metadata/unix-api-server.hpp"

namespace homepi::metadata {

namespace {

std::atomic<bool> g_stop{false};
ServiceEventLoop* g_event_loop = nullptr;

void on_signal(int) {
  g_stop = true;
  if (g_event_loop != nullptr) {
    g_event_loop->request_shutdown();
  }
}

bool snapshot_has_content(const NowPlayingSnapshot& snapshot) {
  return !snapshot.title.empty() || !snapshot.artist.empty() || !snapshot.album.empty() ||
         !snapshot.client_name.empty() || !snapshot.track_id.empty() || snapshot.has_cover_art ||
         (snapshot.playing && snapshot.duration_ms > 0);
}

bool is_client_field(const std::string& field) {
  return field == "client_name" || field == "client_model" || field == "client_user_agent" ||
         field == "client_ip" || field == "client_device_id" || field == "client_mac";
}

bool is_track_field(const std::string& field) {
  return field == "title" || field == "artist" || field == "album" || field == "genre" ||
         field == "composer" || field == "comment" || field == "sort_title" ||
         field == "file_kind" || field == "stream_url";
}

struct PendingMetadataBundle {
  std::string persistent_id;
  std::string title;
  std::string artist;
  std::string album;
  int duration_ms = 0;
  std::vector<std::uint8_t> cover_art_bytes;
  bool has_cover_art = false;
};

}  // namespace

struct Service::Impl {
  NowPlayingState state;
  std::unique_ptr<MetadataStateRepository> repository;
  std::unique_ptr<PipeManager> pipes;
  std::unique_ptr<EventsOwnerSubscriber> events_subscriber;
  std::unique_ptr<MetadataCoalescer> coalescer;
  std::unique_ptr<RealtimeProgressServer> realtime_server;
  std::unique_ptr<UnixApiServer> server;
  std::unique_ptr<ServiceEventLoop> event_loop;
  std::unique_ptr<homepi::events::EventsClient> events_publisher;
  homepi::events::EventEmitter* events = nullptr;
  std::chrono::steady_clock::time_point last_progress_persist_{};
  bool pending_client_update_ = false;
  bool pending_track_update_ = false;
  std::string pending_flush_reason_{"coalesce"};
  PendingMetadataBundle pending_bundle_;
  std::chrono::steady_clock::time_point bundle_opened_at_{};
  bool text_bundle_complete_ = false;
  bool bundle_text_ready_ = false;
  bool cover_received_for_track_ = false;
  std::string committed_track_id_;
  bool committed_with_cover_ = false;
  std::chrono::steady_clock::time_point text_bundle_completed_at_{};
  bool replaying_pipe_cache_ = false;
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

  impl_->repository =
      std::make_unique<MetadataStateRepository>(config_.database_path, config_.cache_dir);

  const auto merge_persisted_now_playing = [this]() {
    if (const auto row = impl_->repository->load_now_playing()) {
      if (row->owner_zone_id <= 0 || !row->playing) {
        return;
      }
      const auto snapshot = impl_->state.snapshot();
      if (snapshot.owner_zone_id <= 0) {
        impl_->state.set_owner_zone(row->owner_zone_id);
        if (impl_->pipes) {
          impl_->pipes->set_owner_zone(row->owner_zone_id);
        }
      }
      const auto apply_if_empty = [&](const std::string& field, const std::string& value) {
        if (value.empty()) {
          return;
        }
        if (field == "title" && snapshot.title.empty()) {
          impl_->state.update_field(row->owner_zone_id, field, value);
        } else if (field == "artist" && snapshot.artist.empty()) {
          impl_->state.update_field(row->owner_zone_id, field, value);
        } else if (field == "album" && snapshot.album.empty()) {
          impl_->state.update_field(row->owner_zone_id, field, value);
        } else if (field == "client_name" && snapshot.client_name.empty() &&
                   !is_persistent_id_like(value)) {
          impl_->state.update_field(row->owner_zone_id, field, value);
        } else if (field == "client_model" && snapshot.client_model.empty()) {
          impl_->state.update_field(row->owner_zone_id, field, value);
        } else if (field == "track_id" && snapshot.track_id.empty()) {
          impl_->state.update_field(row->owner_zone_id, field, value);
        } else if (field == "persistent_id" && snapshot.persistent_id.empty()) {
          impl_->state.update_field(row->owner_zone_id, field, value);
        }
      };
      apply_if_empty("title", row->title);
      apply_if_empty("artist", row->artist);
      apply_if_empty("album", row->album);
      apply_if_empty("client_name", row->client_name);
      apply_if_empty("client_model", row->client_model);
      apply_if_empty("track_id", row->track_id);
      apply_if_empty("persistent_id", row->persistent_id);
      if (impl_->state.snapshot().updated_at.empty() && !row->updated_at.empty()) {
        impl_->state.restore_updated_at(row->updated_at);
      }
      if (snapshot.duration_ms <= 0 && row->duration_ms > 0) {
        impl_->state.update_progress(row->owner_zone_id, row->position_ms, row->duration_ms,
                                     row->playing);
      } else if (snapshot.position_ms <= 0 && row->position_ms > 0) {
        impl_->state.update_progress(row->owner_zone_id, row->position_ms, row->duration_ms,
                                     row->playing);
      } else if (snapshot.playing != row->playing) {
        impl_->state.update_playing(row->owner_zone_id, row->playing);
      }
      if (!snapshot.has_cover_art && row->has_cover_art) {
        const std::string cover_path =
            row->cover_art_path.empty() && !row->cover_art_id.empty()
                ? MetadataStateRepository::artwork_directory(config_.cache_dir) + "/sha256-" +
                      row->cover_art_id + ".jpg"
                : row->cover_art_path;
        impl_->state.mark_cover_art(row->owner_zone_id, row->cover_art_id, cover_path, true);
      }
    }
  };

  const auto persist_track_metadata = [&]() {
    const auto snapshot = impl_->state.snapshot();
    if (snapshot.owner_zone_id > 0) {
      impl_->repository->save_snapshot(snapshot);
    }
  };

  const auto snapshot_for_response = [&]() -> NowPlayingSnapshot {
    return impl_->state.snapshot();
  };

  impl_->server = std::make_unique<UnixApiServer>(
      config_.socket_path,
      [this, snapshot_for_response](const std::string& correlation_id) {
        homepi::events::EventEnvelope envelope;
        envelope.id = config_.service + "-snapshot";
        envelope.source = config_.service;
        envelope.topic = "modules.metadata.snapshot";
        envelope.event = "metadata_snapshot";
        envelope.correlation_id = correlation_id;
        envelope.timestamp = homepi::events::iso_timestamp();
        envelope.payload_json = SnapshotBuilder::build_payload(snapshot_for_response());
        return homepi::events::build_event_line(envelope);
      },
      [this](const std::string& correlation_id, int limit) {
        homepi::events::EventEnvelope envelope;
        envelope.id = config_.service + "-history";
        envelope.source = config_.service;
        envelope.topic = "modules.metadata.history";
        envelope.event = "play_history_snapshot";
        envelope.correlation_id = correlation_id;
        envelope.timestamp = homepi::events::iso_timestamp();
        envelope.payload_json =
            SnapshotBuilder::build_history_payload(impl_->repository->load_play_history(limit));
        return homepi::events::build_event_line(envelope);
      });

  homepi::events::EventEmitter events(config_.service, [this](const std::string& line) {
    if (impl_->server) {
      impl_->server->broadcast(line);
    }
    if (impl_->events_publisher) {
      impl_->events_publisher->publish(line);
    }
  });
  impl_->events = &events;

  impl_->events_publisher =
      std::make_unique<homepi::events::EventsClient>(config_.events_socket, config_.service);
  impl_->events_publisher->start(
      {},
      {"modules.metadata.snapshot", "modules.metadata.now_playing", "modules.metadata.cover_art",
       "modules.metadata.playback", "modules.metadata.history", "core.service", "system.service"},
      [](const std::string&) {});

  const auto emit_play_history_updated = [&](int history_id) {
    const auto entries = impl_->repository->load_play_history(1);
    if (entries.empty() || entries.front().id != history_id) {
      return;
    }
    events.emit("modules.metadata.history", "play_history_updated", "history-updated",
                SnapshotBuilder::build_history_updated_payload(entries.front(), 20));
  };

  const auto finalize_stream = [&](const NowPlayingSnapshot& snapshot) {
    if (!MetadataStateRepository::is_meaningful_stream(snapshot)) {
      return;
    }
    if (const auto history_id = impl_->repository->record_play_history(snapshot)) {
      emit_play_history_updated(*history_id);
    }
  };

  const auto emit_snapshot = [&](const std::string& correlation_id, bool allow_empty = false) {
    impl_->state.normalize_snapshot();
    const auto snapshot = impl_->state.snapshot();
    if (!allow_empty && !snapshot_has_content(snapshot)) {
      return;
    }
    if (!allow_empty && snapshot.title.empty() && snapshot.track_id.empty() &&
        snapshot.persistent_id.empty()) {
      return;
    }
    const std::string payload = SnapshotBuilder::build_payload(snapshot);
    events.emit("modules.metadata.snapshot", "metadata_snapshot", correlation_id, payload);
    events.emit("modules.metadata.now_playing", "metadata_track_changed", correlation_id, payload);
  };

  const auto schedule_persist_snapshot = [&]() {
    if (!impl_->event_loop) {
      persist_track_metadata();
      return;
    }
    impl_->event_loop->arm_metadata_flush_timer(0, [&]() { persist_track_metadata(); });
  };

  const auto flush_coalesced = [&](const std::string& reason) {
    impl_->state.normalize_snapshot();
    const auto snapshot = impl_->state.snapshot();
    if (snapshot.owner_zone_id > 0) {
      impl_->repository->save_snapshot(snapshot);
    }
    if (impl_->pending_client_update_) {
      const std::string payload = SnapshotBuilder::build_payload(snapshot);
      events.emit("modules.metadata.now_playing", "metadata_client_updated", reason, payload);
      impl_->pending_client_update_ = false;
    }
    if (impl_->pending_track_update_ || snapshot_has_content(snapshot)) {
      emit_snapshot(reason);
      impl_->pending_track_update_ = false;
    }
  };

  impl_->coalescer = std::make_unique<MetadataCoalescer>(
      config_.metadata_debounce_ms, [](const std::string&) {});

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

  const auto update_progress_persist_timer = [&]() {
    if (!impl_->event_loop) {
      return;
    }
    const bool playing = impl_->state.snapshot().playing;
    impl_->event_loop->set_progress_persist_timer(
        playing, [&]() { maybe_persist_progress(true); });
  };

  const auto schedule_coalesced_flush = [&](const std::string& reason, bool immediate = false) {
    if (impl_->coalescer->awaiting_bundle_end() && !immediate) {
      return;
    }
    impl_->pending_flush_reason_ = reason;
    if (!impl_->event_loop) {
      flush_coalesced(reason);
      return;
    }
    impl_->event_loop->arm_metadata_flush_timer(
        immediate ? 0 : config_.metadata_debounce_ms,
        [&]() { flush_coalesced(impl_->pending_flush_reason_); });
  };

  const auto ensure_zone_owner = [&](int zone_id) {
    if (zone_id <= 0) {
      return;
    }
    if (impl_->state.snapshot().owner_zone_id == 0) {
      impl_->state.claim_owner_zone(zone_id);
    }
    if (impl_->pipes->owner_zone_id() == 0) {
      impl_->pipes->set_owner_zone(zone_id);
    }
  };

  const auto sync_state_from_pipe_cache = [&](int zone_id) {
    if (zone_id <= 0 || impl_->replaying_pipe_cache_) {
      return;
    }
    impl_->replaying_pipe_cache_ = true;
    ensure_zone_owner(zone_id);
    impl_->pipes->sync_zone_cache(zone_id);
    impl_->replaying_pipe_cache_ = false;
  };

  const auto persist_and_publish_progress = [&](int zone_id, int position_ms, int duration_ms,
                                                bool playing, const std::string& source) {
    ensure_zone_owner(zone_id);
    const auto before = impl_->state.snapshot();
    int effective_duration = duration_ms;
    if (before.duration_ms > 0) {
      effective_duration = -1;
    }
    impl_->state.update_progress(zone_id, position_ms, effective_duration, playing);
    const auto after = impl_->state.snapshot();
    if (after.duration_ms > 0 && before.duration_ms <= 0 && !after.track_id.empty()) {
      emit_snapshot("duration-set");
      schedule_persist_snapshot();
    }
    const auto snapshot = impl_->state.snapshot();
    const std::string track_key = MetadataStateRepository::track_key_for(snapshot);
    if (snapshot.duration_ms > 0 && !track_key.empty()) {
      impl_->repository->cache_track_duration(track_key, snapshot.duration_ms);
    }
    publish_realtime_progress(source);
    maybe_persist_progress(false);
    update_progress_persist_timer();
  };

  const auto apply_cached_duration = [&](int zone_id) {
    const auto snapshot = impl_->state.snapshot();
    if (snapshot.duration_ms > 0) {
      return;
    }
    const std::string track_key = MetadataStateRepository::track_key_for(snapshot);
    if (track_key.empty()) {
      return;
    }
    if (const auto cached = impl_->repository->load_cached_track_duration(track_key)) {
      persist_and_publish_progress(zone_id, -1, *cached, snapshot.playing, "cache:track_duration");
    }
  };

  const auto apply_cover_art_bytes = [&](int zone_id, const std::vector<std::uint8_t>& bytes) {
    if (bytes.empty() || !CoverArtValidator::is_valid_image(bytes)) {
      return false;
    }
    const auto current = impl_->state.snapshot();
    const std::string art_id = MetadataStateRepository::write_cover_art(
        config_.cache_dir, zone_id, bytes, current.cover_art_id);
    if (art_id.empty()) {
      return false;
    }
    const std::string cover_path = MetadataStateRepository::artwork_directory(config_.cache_dir) +
                                   "/sha256-" + art_id + ".jpg";
    if (!impl_->state.mark_cover_art(zone_id, art_id, cover_path, true)) {
      return false;
    }
    impl_->pending_track_update_ = true;
    return true;
  };

  const auto try_commit_track_snapshot = [&](const std::string& reason, bool force_without_cover) {
    const auto snapshot = impl_->state.snapshot();
    if (snapshot.track_id.empty()) {
      return;
    }
    if (snapshot.title.empty() && snapshot.artist.empty()) {
      return;
    }

    const bool same_track = impl_->committed_track_id_ == snapshot.track_id;
    const bool cover_update =
        same_track && snapshot.has_cover_art && !impl_->committed_with_cover_;
    const bool text_commit = !same_track;
    if (!text_commit && !cover_update) {
      return;
    }
    if (text_commit && !impl_->bundle_text_ready_ && !force_without_cover) {
      return;
    }
    if (text_commit && !force_without_cover && !snapshot.has_cover_art) {
      const auto now = std::chrono::steady_clock::now();
      if (now - impl_->text_bundle_completed_at_ < std::chrono::milliseconds(500)) {
        return;
      }
    }

    emit_snapshot("track-commit:" + reason);
    schedule_persist_snapshot();
    publish_realtime_progress("track-commit:" + reason);

    if (snapshot.has_cover_art && !snapshot.cover_art_id.empty()) {
      impl_->repository->update_latest_history_cover(snapshot.track_id, snapshot.cover_art_id,
                                                     snapshot.cover_art_path);
    }

    impl_->committed_track_id_ = snapshot.track_id;
    impl_->committed_with_cover_ = snapshot.has_cover_art || impl_->committed_with_cover_;
    impl_->text_bundle_complete_ = true;
  };

  const auto schedule_cover_wait_commit = [&]() {
    if (!impl_->event_loop) {
      try_commit_track_snapshot("cover-wait", true);
      return;
    }
    impl_->event_loop->arm_metadata_flush_timer(500, [&]() {
      try_commit_track_snapshot("cover-wait", true);
    });
  };

  const auto apply_pending_bundle = [&](int zone_id) {
    if (impl_->pending_bundle_.persistent_id.empty() && impl_->pending_bundle_.title.empty() &&
        impl_->pending_bundle_.artist.empty() && impl_->pending_bundle_.album.empty() &&
        !impl_->pending_bundle_.has_cover_art) {
      return;
    }

    const std::string track_id = !impl_->pending_bundle_.persistent_id.empty()
                                     ? impl_->pending_bundle_.persistent_id
                                     : impl_->state.snapshot().track_id;
    impl_->state.apply_track_metadata(zone_id, track_id, impl_->pending_bundle_.title,
                                      impl_->pending_bundle_.artist, impl_->pending_bundle_.album);

    if (impl_->pending_bundle_.duration_ms > 0) {
      const auto snapshot = impl_->state.snapshot();
      impl_->state.update_progress(zone_id, snapshot.position_ms, impl_->pending_bundle_.duration_ms,
                                   snapshot.playing);
      publish_realtime_progress("bundle:duration");
    }
    if (impl_->pending_bundle_.has_cover_art && !impl_->pending_bundle_.cover_art_bytes.empty()) {
      apply_cover_art_bytes(zone_id, impl_->pending_bundle_.cover_art_bytes);
    }
    impl_->pending_track_update_ = true;
  };

  const auto schedule_late_track_sync = [&](int zone_id) {
    if (!impl_->event_loop) {
      return;
    }
    impl_->event_loop->arm_metadata_flush_timer(750, [&, zone_id]() {
      sync_state_from_pipe_cache(zone_id);
      apply_cached_duration(zone_id);
      const auto snapshot = impl_->state.snapshot();
      if (snapshot.has_cover_art && !impl_->committed_with_cover_) {
        try_commit_track_snapshot("late-cover", false);
      } else if (snapshot.duration_ms > 0 && !snapshot.title.empty()) {
        emit_snapshot("late-duration");
        schedule_persist_snapshot();
        publish_realtime_progress("late-duration");
      }
    });
  };

  const auto begin_track_identity = [&](int zone_id, const std::string& persistent_id) {
    if (persistent_id.empty() || !is_persistent_id_like(persistent_id)) {
      return;
    }
    const auto snapshot = impl_->state.snapshot();
    const bool identity_changed = snapshot.persistent_id != persistent_id;
    if (!impl_->pending_bundle_.persistent_id.empty() &&
        impl_->pending_bundle_.persistent_id != persistent_id) {
      impl_->pending_bundle_ = PendingMetadataBundle{};
    }
    if (identity_changed && !snapshot.persistent_id.empty()) {
      finalize_stream(snapshot);
      impl_->state.clear_track_identity();
      impl_->pending_bundle_ = PendingMetadataBundle{};
      impl_->committed_track_id_.clear();
      impl_->committed_with_cover_ = false;
      impl_->text_bundle_complete_ = false;
      impl_->bundle_text_ready_ = false;
      impl_->cover_received_for_track_ = false;
    }
    impl_->pending_bundle_.persistent_id = persistent_id;
    impl_->state.update_field(zone_id, "persistent_id", persistent_id);
    impl_->state.update_field(zone_id, "track_id", persistent_id);
    apply_cached_duration(zone_id);
    if (identity_changed) {
      impl_->state.touch_started_at();
    }
  };

  const auto apply_field_update = [&](int zone_id, const std::string& field,
                                      const std::string& value) {
    ensure_zone_owner(zone_id);
    if (value.empty() && !is_client_field(field)) {
      return;
    }

    if (field == "track_id" || field == "persistent_id") {
      begin_track_identity(zone_id, value);
      return;
    }

    if (impl_->coalescer->awaiting_bundle_end() && is_track_field(field)) {
      if (field == "title" && !value.empty()) {
        impl_->pending_bundle_.title = value;
      } else if (field == "artist" && !value.empty()) {
        impl_->pending_bundle_.artist = value;
      } else if (field == "album" && !value.empty()) {
        impl_->pending_bundle_.album = value;
      }
      return;
    }

    if (!impl_->state.update_field(zone_id, field, value)) {
      return;
    }
    if (is_client_field(field)) {
      impl_->pending_client_update_ = true;
      schedule_coalesced_flush("field-updated", true);
    }
  };

  const auto persist_and_emit_playing = [&](int zone_id, bool playing) {
    if (!impl_->state.update_playing(zone_id, playing)) {
      return;
    }
    publish_realtime_progress("pipe:playback_state");
    maybe_persist_progress(true);
    update_progress_persist_timer();
    events.emit("modules.metadata.playback", "playback_state_changed", "playback",
                SnapshotBuilder::build_progress_payload(zone_id, impl_->state.snapshot().position_ms,
                                                        impl_->state.snapshot().duration_ms,
                                                        playing));
  };

  const auto handle_owner_change = [&](int owner_zone_id) {
    const int previous_owner = impl_->state.snapshot().owner_zone_id;
    const auto previous_snapshot = impl_->state.snapshot_for_finalize();
    const bool changed = impl_->state.set_owner_zone(owner_zone_id);
    if (!changed) {
      return;
    }
    impl_->pipes->set_owner_zone(owner_zone_id);

    if (previous_owner > 0 && previous_owner != owner_zone_id) {
      finalize_stream(previous_snapshot);
      impl_->repository->clear_owner(previous_owner);
    }

    if (owner_zone_id <= 0) {
      events.emit("modules.metadata.now_playing", "metadata_owner_changed", "owner-cleared",
                  SnapshotBuilder::build_owner_changed_payload(0, previous_owner));
      events.emit("modules.metadata.now_playing", "metadata_cleared", "owner-cleared", "{}");
      emit_snapshot("owner-cleared", true);
      publish_realtime_progress("owner-cleared");
      return;
    }

    events.emit("modules.metadata.now_playing", "metadata_owner_changed", "owner-changed",
                SnapshotBuilder::build_owner_changed_payload(owner_zone_id, previous_owner));

    if (const auto restored = impl_->repository->load_owner(owner_zone_id)) {
      const auto snapshot = impl_->state.snapshot();
      const auto fill_if_empty = [&](const std::string& field, const std::string& value) {
        if (value.empty()) {
          return;
        }
        if (field == "title" && snapshot.title.empty()) {
          impl_->state.update_field(owner_zone_id, field, value);
        } else if (field == "artist" && snapshot.artist.empty()) {
          impl_->state.update_field(owner_zone_id, field, value);
        } else if (field == "album" && snapshot.album.empty()) {
          impl_->state.update_field(owner_zone_id, field, value);
        } else if (field == "client_name" && snapshot.client_name.empty() &&
                   !is_persistent_id_like(value)) {
          impl_->state.update_field(owner_zone_id, field, value);
        } else if (field == "client_model" && snapshot.client_model.empty()) {
          impl_->state.update_field(owner_zone_id, field, value);
        }
      };
      fill_if_empty("title", restored->title);
      fill_if_empty("artist", restored->artist);
      fill_if_empty("album", restored->album);
      fill_if_empty("client_name", restored->client_name);
      fill_if_empty("client_model", restored->client_model);
      if (snapshot.track_id.empty() && !restored->track_id.empty()) {
        impl_->state.update_field(owner_zone_id, "track_id", restored->track_id);
      }
      if (snapshot.persistent_id.empty() && !restored->persistent_id.empty()) {
        impl_->state.update_field(owner_zone_id, "persistent_id", restored->persistent_id);
      }
      if (snapshot.duration_ms <= 0 && restored->duration_ms > 0) {
        impl_->state.update_progress(owner_zone_id, restored->position_ms, restored->duration_ms,
                                     restored->playing);
      } else if (snapshot.position_ms <= 0 && restored->position_ms > 0) {
        impl_->state.update_progress(owner_zone_id, restored->position_ms, restored->duration_ms,
                                     restored->playing);
      }
      if (!snapshot.has_cover_art && restored->has_cover_art) {
        const std::string cover_path =
            restored->cover_art_path.empty() && !restored->cover_art_id.empty()
                ? MetadataStateRepository::artwork_directory(config_.cache_dir) + "/sha256-" +
                      restored->cover_art_id + ".jpg"
                : restored->cover_art_path;
        impl_->state.mark_cover_art(owner_zone_id, restored->cover_art_id, cover_path, true);
      }
      apply_cached_duration(owner_zone_id);
    }
    impl_->pending_track_update_ = true;
    flush_coalesced("owner-changed");
    sync_state_from_pipe_cache(owner_zone_id);
    publish_realtime_progress("owner-changed");
  };

  const auto handle_routing_context = [&](const std::string& payload, const std::string& /*event*/) {
    const std::vector<int> active_stack = parse_int_array_field(payload, "activeStack");
    const int payload_owner = parse_int_field(payload, "ownerZoneId");
    if (active_stack.empty() && payload_owner <= 0) {
      if (impl_->state.snapshot().owner_zone_id > 0) {
        handle_owner_change(0);
      }
      return;
    }
    if (active_stack.empty()) {
      return;
    }
    const int current_owner = impl_->state.snapshot().owner_zone_id;
    const bool owner_in_stack =
        current_owner > 0 &&
        std::find(active_stack.begin(), active_stack.end(), current_owner) != active_stack.end();
    if (!owner_in_stack) {
      handle_owner_change(active_stack.front());
    }
  };

  impl_->pipes = std::make_unique<PipeManager>();
  const auto force_end_open_bundle = [&](int zone_id) {
    if (!impl_->coalescer->awaiting_bundle_end()) {
      return;
    }
    impl_->coalescer->set_awaiting_bundle_end(false);
    apply_pending_bundle(zone_id);
    impl_->pending_bundle_ = PendingMetadataBundle{};
    impl_->bundle_text_ready_ = true;
    impl_->text_bundle_completed_at_ = std::chrono::steady_clock::now();
    try_commit_track_snapshot("bundle-force-end", false);
    if (!impl_->state.snapshot().has_cover_art) {
      schedule_cover_wait_commit();
    }
  };

  PipeManagerCallbacks pipe_callbacks;
  pipe_callbacks.on_field = apply_field_update;
  pipe_callbacks.on_progress = [&](int zone_id, int position_ms, int duration_ms, bool playing) {
    if (impl_->coalescer->awaiting_bundle_end() && duration_ms > 0) {
      impl_->pending_bundle_.duration_ms = duration_ms;
      return;
    }
    persist_and_publish_progress(zone_id, position_ms, duration_ms, playing, "pipe:ssnc/prgr");
  };
  pipe_callbacks.on_playback_state = persist_and_emit_playing;
  pipe_callbacks.on_cover_art = [&](int zone_id, const std::vector<std::uint8_t>& bytes) {
    ensure_zone_owner(zone_id);
    if (bytes.size() > 2 * 1024 * 1024) {
      return;
    }
    if (impl_->coalescer->awaiting_bundle_end()) {
      impl_->pending_bundle_.cover_art_bytes = bytes;
      impl_->pending_bundle_.has_cover_art = true;
      impl_->cover_received_for_track_ = true;
      return;
    }
    if (!apply_cover_art_bytes(zone_id, bytes)) {
      return;
    }
    impl_->cover_received_for_track_ = true;
    if (impl_->bundle_text_ready_) {
      try_commit_track_snapshot("cover-after-text", false);
    }
  };
  pipe_callbacks.on_metadata_bundle_start = [&](int zone_id) {
    ensure_zone_owner(zone_id);
    if (impl_->coalescer->awaiting_bundle_end()) {
      force_end_open_bundle(zone_id);
    }
    impl_->coalescer->set_awaiting_bundle_end(true);
    impl_->bundle_opened_at_ = std::chrono::steady_clock::now();
    impl_->bundle_text_ready_ = false;
    impl_->state.begin_metadata_bundle();
  };
  pipe_callbacks.on_metadata_bundle_end = [&](int zone_id) {
    ensure_zone_owner(zone_id);
    impl_->coalescer->set_awaiting_bundle_end(false);
    apply_pending_bundle(zone_id);
    impl_->pending_bundle_ = PendingMetadataBundle{};
    apply_cached_duration(zone_id);
    impl_->bundle_text_ready_ = true;
    impl_->text_bundle_completed_at_ = std::chrono::steady_clock::now();
    try_commit_track_snapshot("bundle-end", false);
    if (!impl_->state.snapshot().has_cover_art && !impl_->cover_received_for_track_) {
      schedule_cover_wait_commit();
    }
    schedule_late_track_sync(zone_id);
  };
  pipe_callbacks.on_session_cleared = [&](int zone_id) {
    if (zone_id != impl_->state.snapshot().owner_zone_id) {
      return;
    }
    const auto snapshot = impl_->state.snapshot();
    if (snapshot.duration_ms <= 0 && snapshot.position_ms > 0) {
      persist_and_publish_progress(zone_id, snapshot.position_ms, snapshot.position_ms, false,
                                   "pipe:session_end");
      const std::string track_key = MetadataStateRepository::track_key_for(snapshot);
      if (!track_key.empty()) {
        impl_->repository->cache_track_duration(track_key, snapshot.position_ms);
      }
    }
    finalize_stream(impl_->state.snapshot_for_finalize());
    impl_->state.update_playing(zone_id, false);
    persist_track_metadata();
    publish_realtime_progress("pipe:session_end");
    update_progress_persist_timer();
  };
  impl_->pipes->prepare(config_.pipe_prefix, config_.zone_count, std::move(pipe_callbacks));

  merge_persisted_now_playing();

  impl_->events_subscriber = std::make_unique<EventsOwnerSubscriber>();
  impl_->events_subscriber->start(
      config_.events_socket, config_.service, handle_owner_change,
      [this](const std::vector<int>& enabled_zone_ids) {
        impl_->pipes->set_enabled_zones(enabled_zone_ids);
      },
      handle_routing_context);

  if (impl_->events_subscriber->owner_zone_id() > 0) {
    handle_owner_change(impl_->events_subscriber->owner_zone_id());
  }

  if (!impl_->realtime_server->prepare(config_.realtime_socket_path)) {
    logger.log(homepi::logging::LogLevel::ERROR, "metadata.runtime", "realtime_socket_failed",
               "startup", "failed to bind audio-realtime socket");
    return 1;
  }

  if (!impl_->server->start()) {
    logger.log(homepi::logging::LogLevel::ERROR, "metadata.runtime", "socket_start_failed",
               "startup", "failed to bind metadata unix socket");
    return 1;
  }

  impl_->event_loop = std::make_unique<ServiceEventLoop>();
  if (!impl_->event_loop->init()) {
    logger.log(homepi::logging::LogLevel::ERROR, "metadata.runtime", "event_loop_failed",
               "startup", "failed to initialize epoll event loop");
    return 1;
  }
  g_event_loop = impl_->event_loop.get();

  impl_->pipes->attach(*impl_->event_loop);
  impl_->realtime_server->attach(*impl_->event_loop);

  impl_->event_loop->start_maintenance_timer([&]() {
    impl_->pipes->open_missing_pipes();
    impl_->repository->cleanup_unreferenced_artwork();
    if (impl_->coalescer->awaiting_bundle_end()) {
      const auto now = std::chrono::steady_clock::now();
      if (now - impl_->bundle_opened_at_ > std::chrono::milliseconds(800)) {
        const int zone_id = impl_->state.snapshot().owner_zone_id;
        if (zone_id > 0) {
          force_end_open_bundle(zone_id);
        }
      }
    }
  });
  update_progress_persist_timer();

  events.emit_service_status("service_started", "startup", "healthy");
  events.emit_service_status("service_ready", "startup", "healthy");
  if (const auto persisted = impl_->repository->load_now_playing()) {
    if (persisted->owner_zone_id > 0) {
      impl_->state.apply_persisted_snapshot(*persisted);
      if (impl_->pipes) {
        impl_->pipes->set_owner_zone(persisted->owner_zone_id);
      }
    }
  } else {
    merge_persisted_now_playing();
  }
  publish_realtime_progress("startup");
  emit_snapshot("startup", true);

  impl_->event_loop->run([&]() { return g_stop.load(); });

  g_event_loop = nullptr;
  shutdown();
  events.emit_service_status("service_stopped", "shutdown", "offline");
  return 0;
}

void Service::shutdown() {
  if (impl_->event_loop) {
    impl_->event_loop->request_shutdown();
  }
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
  if (impl_->events_publisher) {
    impl_->events_publisher->stop();
    impl_->events_publisher.reset();
  }
}

}  // namespace homepi::metadata
