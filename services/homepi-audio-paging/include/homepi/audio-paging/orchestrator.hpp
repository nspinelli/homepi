#pragma once

#include <mutex>
#include <string>

#include "homepi/audio-paging/types.hpp"

namespace homepi::events {
class EventEmitter;
}

namespace homepi::audio_paging {

class ChimePlayer;
class HifiPagingController;
class PagingRepository;
class PcmPlayer;
class PiperWorker;
class ResourceManager;

/** Coordinates speak/chime sequences and emits audio.paging lifecycle events. */
class Orchestrator {
 public:
  /** Creates orchestrator from service module dependencies. */
  Orchestrator(PagingRepository* repository, PiperWorker* piper_worker, PcmPlayer* pcm_player,
               ChimePlayer* chime_player, HifiPagingController* hifi_controller,
               ResourceManager* resource_manager, homepi::events::EventEmitter* emitter,
               int page_on_timeout_ms = 2000, int page_off_timeout_ms = 2000);

  /** Handles a speak command payload from broker subscription. */
  void handle_speak(const SpeakCommand& command, const PagingStatus& status);

  /** Handles a chime-only command payload from broker subscription. */
  void handle_chime(const ChimeCommand& command, const PagingStatus& status);

  /** Plays a chime through the paging DAC only (no HiFi PAGE). */
  bool handle_preview_chime(const ChimeCommand& command);

  /** Handles installed/catalog voice preview playback on paging DAC only. */
  bool handle_preview_voice(const PreviewVoiceCommand& command);

  /** Reloads Piper worker with selected or active voice. */
  bool handle_reload_voice(const ReloadVoiceCommand& command);

  /** Returns true while an orchestrated job is in progress. */
  bool is_busy() const;

 private:
  std::string create_job_id(const std::string& prefix) const;
  void emit_event(const std::string& event_name, const std::string& correlation_id,
                  const std::string& payload_json) const;

  PagingRepository* repository_ = nullptr;
  PiperWorker* piper_worker_ = nullptr;
  PcmPlayer* pcm_player_ = nullptr;
  ChimePlayer* chime_player_ = nullptr;
  HifiPagingController* hifi_controller_ = nullptr;
  ResourceManager* resource_manager_ = nullptr;
  homepi::events::EventEmitter* emitter_ = nullptr;
  int page_on_timeout_ms_ = 2000;
  int page_off_timeout_ms_ = 2000;
  mutable std::mutex mutex_;
  bool busy_ = false;
};

}  // namespace homepi::audio_paging
