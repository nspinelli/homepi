#include "homepi/audio-paging/orchestrator.hpp"

#include <algorithm>
#include <chrono>
#include <filesystem>
#include <thread>

#include "homepi/audio-paging/chime-player.hpp"
#include "homepi/audio-paging/hifi-paging-controller.hpp"
#include "homepi/audio-paging/json-utils.hpp"
#include "homepi/audio-paging/paging-repository.hpp"
#include "homepi/audio-paging/pcm-player.hpp"
#include "homepi/audio-paging/piper-worker.hpp"
#include "homepi/audio-paging/resource-manager.hpp"
#include "homepi/events/event-emitter.hpp"

namespace fs = std::filesystem;

namespace homepi::audio_paging {

namespace {

constexpr int kMinPagePlaybackSettleMs = 0;

int page_playback_settle_ms(const PagingConfig& config) {
  return std::max(config.min_preroll_ms, kMinPagePlaybackSettleMs);
}

}  // namespace

Orchestrator::Orchestrator(PagingRepository* repository, PiperWorker* piper_worker,
                           PcmPlayer* pcm_player, ChimePlayer* chime_player,
                           HifiPagingController* hifi_controller, ResourceManager* resource_manager,
                           homepi::events::EventEmitter* emitter, int page_on_timeout_ms,
                           int page_off_timeout_ms)
    : repository_(repository),
      piper_worker_(piper_worker),
      pcm_player_(pcm_player),
      chime_player_(chime_player),
      hifi_controller_(hifi_controller),
      resource_manager_(resource_manager),
      emitter_(emitter),
      page_on_timeout_ms_(page_on_timeout_ms),
      page_off_timeout_ms_(page_off_timeout_ms) {}

std::string Orchestrator::create_job_id(const std::string& prefix) const {
  const auto now = std::chrono::steady_clock::now().time_since_epoch().count();
  return prefix + "-" + std::to_string(now);
}

void Orchestrator::emit_event(const std::string& event_name, const std::string& correlation_id,
                              const std::string& payload_json) const {
  if (emitter_ == nullptr) {
    return;
  }
  emitter_->emit("audio.paging", event_name, correlation_id, payload_json);
}

void Orchestrator::handle_speak(const SpeakCommand& command, const PagingStatus& status) {
  std::lock_guard<std::mutex> lock(mutex_);
  if (busy_) {
    emit_event("audio.paging.busy", command.correlation_id, "{\"reason\":\"paging_busy\"}");
    return;
  }
  busy_ = true;
  const std::string job_id = create_job_id("speak");
  const PagingConfig config = repository_->get_config();
  const std::string voice_id = command.voice_id.empty() ? config.active_voice_id : command.voice_id;
  const std::string chime_id = command.chime_id.empty() ? config.active_chime_id : command.chime_id;
  repository_->create_job(job_id, command.correlation_id, command.source, "speak",
                          static_cast<int>(command.text.size()), voice_id, chime_id,
                          command.include_chime);

  emit_event("audio.paging.requested", command.correlation_id,
             "{\"jobId\":\"" + job_id + "\",\"jobType\":\"speak\"}");
  resource_manager_->on_job_started();

  const auto voice = repository_->get_voice(voice_id);
  if (!voice.has_value()) {
    emit_event("audio.paging.failed", command.correlation_id,
               "{\"jobId\":\"" + job_id + "\",\"stage\":\"voice\",\"message\":\"voice_not_installed\"}");
    repository_->mark_job_failed(job_id, "voice", "voice_not_installed");
    busy_ = false;
    resource_manager_->force_cold();
    return;
  }

  emit_event("audio.paging.tts.started", command.correlation_id, "{\"jobId\":\"" + job_id + "\"}");
  const auto output =
      piper_worker_->synthesize(command.text, voice->model_path, voice->config_path,
                                config.keep_last_audio_for_debug);
  if (!output.has_value()) {
    emit_event("audio.paging.failed", command.correlation_id,
               "{\"jobId\":\"" + job_id + "\",\"stage\":\"tts\",\"message\":\"piper_failed\"}");
    repository_->mark_job_failed(job_id, "tts", "piper_failed");
    busy_ = false;
    resource_manager_->force_cold();
    return;
  }
  emit_event("audio.paging.tts.buffer_ready", command.correlation_id,
             "{\"jobId\":\"" + job_id + "\"}");

  if (!status.dac_connected || hifi_controller_->external_page_active()) {
    emit_event("audio.paging.failed", command.correlation_id,
               "{\"jobId\":\"" + job_id +
                   "\",\"stage\":\"readiness\",\"message\":\"paging_not_ready\"}");
    repository_->mark_job_failed(job_id, "readiness", "paging_not_ready");
    if (fs::exists(output->raw_path)) {
      fs::remove(output->raw_path);
    }
    busy_ = false;
    resource_manager_->on_job_finished(config);
    return;
  }

  emit_event("audio.paging.hifi.page_on_requested", command.correlation_id,
             "{\"jobId\":\"" + job_id + "\"}");
  hifi_controller_->request_page_on(command.correlation_id);
  if (!hifi_controller_->wait_for_page_playback_ready(page_on_timeout_ms_,
                                                      page_playback_settle_ms(config))) {
    emit_event("audio.paging.failed", command.correlation_id,
               "{\"jobId\":\"" + job_id + "\",\"stage\":\"page_on\",\"message\":\"page_on_timeout\"}");
    repository_->mark_job_failed(job_id, "page_on", "page_on_timeout");
    if (fs::exists(output->raw_path)) {
      fs::remove(output->raw_path);
    }
    busy_ = false;
    resource_manager_->on_job_finished(config);
    return;
  }
  emit_event("audio.paging.hifi.page_on_confirmed", command.correlation_id,
             "{\"jobId\":\"" + job_id + "\"}");

  if (command.include_chime) {
    if (const auto chime = repository_->get_chime(chime_id)) {
      emit_event("audio.paging.chime.started", command.correlation_id, "{\"jobId\":\"" + job_id + "\"}");
      chime_player_->play(chime->file_path);
      emit_event("audio.paging.chime.completed", command.correlation_id,
                 "{\"jobId\":\"" + job_id + "\"}");
    }
  }

  emit_event("audio.paging.playback.started", command.correlation_id,
             "{\"jobId\":\"" + job_id + "\",\"jobType\":\"speak\"}");
  const bool played = pcm_player_->play_raw_file(output->raw_path, output->sample_rate_hz);
  if (!config.keep_last_audio_for_debug && fs::exists(output->raw_path)) {
    fs::remove(output->raw_path);
  }
  if (!played) {
    emit_event("audio.paging.failed", command.correlation_id,
               "{\"jobId\":\"" + job_id + "\",\"stage\":\"playback\",\"message\":\"aplay_failed\"}");
    repository_->mark_job_failed(job_id, "playback", "aplay_failed");
    hifi_controller_->request_page_off(command.correlation_id);
    hifi_controller_->wait_for_page_off(page_off_timeout_ms_);
    busy_ = false;
    resource_manager_->on_job_finished(config);
    return;
  }
  emit_event("audio.paging.playback.completed", command.correlation_id, "{\"jobId\":\"" + job_id + "\"}");

  emit_event("audio.paging.hifi.page_off_requested", command.correlation_id,
             "{\"jobId\":\"" + job_id + "\"}");
  hifi_controller_->request_page_off(command.correlation_id);
  hifi_controller_->wait_for_page_off(page_off_timeout_ms_);
  emit_event("audio.paging.hifi.page_off_confirmed", command.correlation_id,
             "{\"jobId\":\"" + job_id + "\"}");

  emit_event("audio.paging.completed", command.correlation_id,
             "{\"jobId\":\"" + job_id + "\",\"jobType\":\"speak\"}");
  repository_->mark_job_completed(job_id);
  busy_ = false;
  resource_manager_->on_job_finished(config);
}

void Orchestrator::handle_chime(const ChimeCommand& command, const PagingStatus& status) {
  std::lock_guard<std::mutex> lock(mutex_);
  if (busy_) {
    emit_event("audio.paging.busy", command.correlation_id, "{\"reason\":\"paging_busy\"}");
    return;
  }
  if (!status.dac_connected || hifi_controller_->external_page_active()) {
    emit_event("audio.paging.failed", command.correlation_id,
               "{\"stage\":\"readiness\",\"message\":\"paging_not_ready\"}");
    return;
  }

  busy_ = true;
  const std::string job_id = create_job_id("chime");
  const PagingConfig config = repository_->get_config();
  const std::string chime_id =
      command.chime_id.empty() ? config.active_chime_id : command.chime_id;
  const auto chime = repository_->get_chime(chime_id);
  if (!chime.has_value()) {
    emit_event("audio.paging.failed", command.correlation_id,
               "{\"jobId\":\"" + job_id + "\",\"stage\":\"chime\",\"message\":\"chime_not_found\"}");
    busy_ = false;
    return;
  }

  repository_->create_job(job_id, command.correlation_id, command.source, "chime", 0, "", chime_id,
                          true);
  emit_event("audio.paging.requested", command.correlation_id,
             "{\"jobId\":\"" + job_id + "\",\"jobType\":\"chime\"}");
  resource_manager_->on_job_started();

  emit_event("audio.paging.hifi.page_on_requested", command.correlation_id,
             "{\"jobId\":\"" + job_id + "\"}");
  hifi_controller_->request_page_on(command.correlation_id);
  if (!hifi_controller_->wait_for_page_playback_ready(page_on_timeout_ms_,
                                                      page_playback_settle_ms(config))) {
    emit_event("audio.paging.failed", command.correlation_id,
               "{\"jobId\":\"" + job_id + "\",\"stage\":\"page_on\",\"message\":\"page_on_timeout\"}");
    repository_->mark_job_failed(job_id, "page_on", "page_on_timeout");
    busy_ = false;
    resource_manager_->on_job_finished(config);
    return;
  }
  emit_event("audio.paging.hifi.page_on_confirmed", command.correlation_id,
             "{\"jobId\":\"" + job_id + "\"}");

  emit_event("audio.paging.chime.started", command.correlation_id, "{\"jobId\":\"" + job_id + "\"}");
  const bool chime_played = chime_player_->play(chime->file_path);
  if (!chime_played) {
    emit_event("audio.paging.failed", command.correlation_id,
               "{\"jobId\":\"" + job_id + "\",\"stage\":\"playback\",\"message\":\"aplay_failed\"}");
    repository_->mark_job_failed(job_id, "playback", "aplay_failed");
    hifi_controller_->request_page_off(command.correlation_id);
    hifi_controller_->wait_for_page_off(page_off_timeout_ms_);
    busy_ = false;
    resource_manager_->on_job_finished(config);
    return;
  }
  emit_event("audio.paging.chime.completed", command.correlation_id,
             "{\"jobId\":\"" + job_id + "\"}");

  emit_event("audio.paging.hifi.page_off_requested", command.correlation_id,
             "{\"jobId\":\"" + job_id + "\"}");
  hifi_controller_->request_page_off(command.correlation_id);
  hifi_controller_->wait_for_page_off(page_off_timeout_ms_);
  emit_event("audio.paging.hifi.page_off_confirmed", command.correlation_id,
             "{\"jobId\":\"" + job_id + "\"}");

  emit_event("audio.paging.completed", command.correlation_id,
             "{\"jobId\":\"" + job_id + "\",\"jobType\":\"chime\"}");
  repository_->mark_job_completed(job_id);
  busy_ = false;
  resource_manager_->on_job_finished(config);
}

bool Orchestrator::handle_preview_chime(const ChimeCommand& command) {
  if (busy_) {
    emit_event("audio.paging.busy", command.correlation_id, "{\"reason\":\"paging_busy\"}");
    return false;
  }
  const std::string chime_id =
      command.chime_id.empty() ? repository_->get_config().active_chime_id : command.chime_id;
  const auto chime = repository_->get_chime(chime_id);
  if (!chime.has_value()) {
    emit_event("audio.paging.failed", command.correlation_id,
               "{\"stage\":\"chime\",\"message\":\"chime_not_found\"}");
    return false;
  }
  emit_event("audio.paging.chime.started", command.correlation_id,
             "{\"chimeId\":\"" + json_escape(chime_id) + "\",\"preview\":true}");
  const bool ok = chime_player_->play(chime->file_path);
  if (ok) {
    emit_event("audio.paging.chime.completed", command.correlation_id,
               "{\"chimeId\":\"" + json_escape(chime_id) + "\",\"preview\":true}");
  } else {
    emit_event("audio.paging.failed", command.correlation_id,
               "{\"stage\":\"playback\",\"message\":\"aplay_failed\"}");
  }
  return ok;
}

bool Orchestrator::handle_preview_voice(const PreviewVoiceCommand& command) {
  if (busy_) {
    return false;
  }
  const auto voice = repository_->get_voice(command.voice_id);
  if (!voice.has_value()) {
    return false;
  }
  const PagingConfig config = repository_->get_config();
  const auto output =
      piper_worker_->synthesize(command.text, voice->model_path, voice->config_path,
                                config.keep_last_audio_for_debug);
  if (!output.has_value()) {
    return false;
  }
  const bool ok = pcm_player_->play_raw_file(output->raw_path, output->sample_rate_hz);
  if (!config.keep_last_audio_for_debug && fs::exists(output->raw_path)) {
    fs::remove(output->raw_path);
  }
  return ok;
}

bool Orchestrator::handle_reload_voice(const ReloadVoiceCommand& command) {
  const std::string voice_id =
      command.voice_id.empty() ? repository_->get_config().active_voice_id : command.voice_id;
  const auto voice = repository_->get_voice(voice_id);
  if (!voice.has_value()) {
    return false;
  }
  emit_event("audio.paging.voice.warming", command.correlation_id,
             "{\"voiceId\":\"" + json_escape(voice_id) + "\"}");
  const bool ok = piper_worker_->warm(voice->model_path, voice->config_path);
  if (ok) {
    emit_event("audio.paging.voice.ready", command.correlation_id,
               "{\"voiceId\":\"" + json_escape(voice_id) + "\"}");
  }
  return ok;
}

bool Orchestrator::is_busy() const {
  std::lock_guard<std::mutex> lock(mutex_);
  return busy_;
}

}  // namespace homepi::audio_paging
