#include "homepi/pcm-router/audio-bridge.hpp"

#include <alsa/asoundlib.h>
#include <chrono>
#include <cstring>
#include <thread>
#include <vector>

#include "homepi/pcm-router/loopback-mapping.hpp"

namespace homepi::pcm_router {

namespace {

snd_pcm_format_t to_alsa_format(homepi::storage::SampleFormat format) {
  return format == homepi::storage::SampleFormat::S32Le ? SND_PCM_FORMAT_S32_LE
                                                        : SND_PCM_FORMAT_S16_LE;
}

bool configure_pcm(snd_pcm_t* handle, const AudioProfileTuple& profile, uint32_t period_frames,
                   uint32_t buffer_frames, std::string& error) {
  snd_pcm_hw_params_t* params = nullptr;
  snd_pcm_hw_params_alloca(&params);
  snd_pcm_hw_params_any(handle, params);
  snd_pcm_hw_params_set_access(handle, params, SND_PCM_ACCESS_RW_INTERLEAVED);
  snd_pcm_hw_params_set_format(handle, params, to_alsa_format(profile.sample_format));
  snd_pcm_hw_params_set_channels(handle, params, profile.channels);
  unsigned int rate = profile.sample_rate;
  if (snd_pcm_hw_params_set_rate_near(handle, params, &rate, nullptr) < 0 || rate != profile.sample_rate) {
    error = "ALSA rate negotiation failed";
    return false;
  }
  snd_pcm_uframes_t period = period_frames;
  snd_pcm_uframes_t buffer = buffer_frames;
  snd_pcm_hw_params_set_period_size_near(handle, params, &period, nullptr);
  snd_pcm_hw_params_set_buffer_size_near(handle, params, &buffer);
  if (snd_pcm_hw_params(handle, params) < 0) {
    error = "snd_pcm_hw_params failed";
    return false;
  }
  unsigned int actual_rate = 0;
  snd_pcm_hw_params_get_rate(params, &actual_rate, nullptr);
  if (actual_rate != profile.sample_rate) {
    error = "actual sample rate mismatch";
    return false;
  }
  snd_pcm_prepare(handle);
  return true;
}

int64_t steady_now_ms() {
  return std::chrono::duration_cast<std::chrono::milliseconds>(
             std::chrono::steady_clock::now().time_since_epoch())
      .count();
}

}  // namespace

AudioBridge::AudioBridge(const ServiceConfig& config) : config_(config) {
  for (auto& mode : zone_modes_) {
    mode.store(ZoneCaptureMode::Off);
  }
  rings_.reserve(config.zone_count);
  for (int i = 0; i < config.zone_count; ++i) {
    rings_.push_back(std::make_unique<FrameRing>());
  }
}

AudioBridge::~AudioBridge() { stop(); }

size_t AudioBridge::bytes_per_frame() const { return bytes_per_frame_; }

bool AudioBridge::is_running() const { return running_.load(); }
DacLifecycleState AudioBridge::dac_state() const { return dac_state_.load(); }
AudioBridgeState AudioBridge::bridge_state() const { return bridge_state_.load(); }
AudioBridgeStats AudioBridge::stats() const { return stats_; }
ActiveAudioConfig AudioBridge::active_config() const { return active_config_; }

void AudioBridge::apply_zone_modes(const std::array<ZoneCaptureMode, kMaxZones + 1>& modes) {
  const int playback_zone = playback_owner_.load();
  const int handoff_zone = handoff_source_zone_.load();
  for (int i = 1; i <= kMaxZones; ++i) {
    const ZoneCaptureMode previous = zone_modes_[i].load();
    if (previous == ZoneCaptureMode::Buffer &&
        (modes[i] == ZoneCaptureMode::Drain || modes[i] == ZoneCaptureMode::Disabled ||
         modes[i] == ZoneCaptureMode::Off) &&
        i <= config_.zone_count && i != playback_zone && i != handoff_zone) {
      rings_[static_cast<size_t>(i - 1)]->clear();
    }
    zone_modes_[i].store(modes[i]);
    if (i > config_.zone_count) {
      continue;
    }
    if (modes[i] == ZoneCaptureMode::Buffer) {
      ensure_zone_capture_open(i);
    } else if (modes[i] == ZoneCaptureMode::Disabled) {
      disable_zone_capture(i);
    }
  }
  routing_revision_.fetch_add(1, std::memory_order_relaxed);
}

void AudioBridge::prewarm_zone_capture(int zone_id) {
  if (zone_id < 1 || zone_id > config_.zone_count) {
    return;
  }
  capture_slots_[zone_id].idle_close_at_ms.store(0);
  ensure_zone_capture_open(zone_id);
}

void AudioBridge::disable_zone_capture(int zone_id) {
  close_zone_capture_internal(zone_id);
}

void AudioBridge::schedule_zone_capture_idle_close(int zone_id) {
  if (zone_id < 1 || zone_id > config_.zone_count) {
    return;
  }
  ZoneCaptureSlot& slot = capture_slots_[zone_id];
  if (!slot.running.load()) {
    slot.idle_close_at_ms.store(0);
    return;
  }
  slot.idle_close_at_ms.store(steady_now_ms() + config_.capture_idle_close_delay_ms);
}

CaptureLifecycleTick AudioBridge::tick_capture_lifecycle() {
  CaptureLifecycleTick tick;
  const int64_t now_ms = steady_now_ms();
  const int owner = playback_owner_.load();
  for (int zone_id = 1; zone_id <= config_.zone_count; ++zone_id) {
    ZoneCaptureSlot& slot = capture_slots_[zone_id];
    const int64_t close_at = slot.idle_close_at_ms.load();
    if (close_at <= 0 || now_ms < close_at) {
      continue;
    }
    if (zone_id == owner) {
      continue;
    }
    if (zone_modes_[zone_id].load() == ZoneCaptureMode::Buffer) {
      continue;
    }
    if (!slot.running.load()) {
      slot.idle_close_at_ms.store(0);
      continue;
    }
    close_zone_capture_internal(zone_id);
    tick.closed_zones.push_back(zone_id);
  }
  return tick;
}

std::vector<int> AudioBridge::open_capture_zones() const {
  std::vector<int> zones;
  for (int zone_id = 1; zone_id <= config_.zone_count; ++zone_id) {
    if (capture_slots_[zone_id].running.load()) {
      zones.push_back(zone_id);
    }
  }
  return zones;
}

std::vector<int> AudioBridge::closing_grace_capture_zones() const {
  std::vector<int> zones;
  for (int zone_id = 1; zone_id <= config_.zone_count; ++zone_id) {
    if (capture_slots_[zone_id].idle_close_at_ms.load() > 0 &&
        capture_slots_[zone_id].running.load()) {
      zones.push_back(zone_id);
    }
  }
  return zones;
}

bool AudioBridge::ensure_zone_capture_open(int zone_id) {
  if (!running_.load() || zone_id < 1 || zone_id > config_.zone_count) {
    return false;
  }
  ZoneCaptureSlot& slot = capture_slots_[zone_id];
  if (slot.running.load()) {
    slot.idle_close_at_ms.store(0);
    return true;
  }

  const auto mapped = map_zone_capture_device(zone_id, config_);
  if (!mapped.has_value()) {
    return false;
  }

  snd_pcm_t* handle = nullptr;
  if (snd_pcm_open(&handle, mapped->capture.c_str(), SND_PCM_STREAM_CAPTURE, 0) < 0) {
    return false;
  }
  std::string error;
  if (!configure_pcm(handle, active_config_.loopback_profile, config_.period_frames,
                     config_.buffer_frames, error)) {
    snd_pcm_close(handle);
    return false;
  }

  rings_[static_cast<size_t>(zone_id - 1)]->init(config_.period_frames * 8, bytes_per_frame_);
  {
    std::lock_guard lock(slot.mutex);
    slot.handle = handle;
  }
  slot.stop_requested.store(false);
  slot.running.store(true);
  const int zone_index = zone_id - 1;
  slot.thread = std::thread([this, zone_index]() { capture_loop(zone_index); });
  return true;
}

void AudioBridge::close_zone_capture_internal(int zone_id) {
  if (zone_id < 1 || zone_id > config_.zone_count) {
    return;
  }
  ZoneCaptureSlot& slot = capture_slots_[zone_id];
  slot.idle_close_at_ms.store(0);
  if (!slot.running.load()) {
    return;
  }
  slot.stop_requested.store(true);
  if (slot.thread.joinable()) {
    slot.thread.join();
  }
  slot.stop_requested.store(false);
  rings_[static_cast<size_t>(zone_id - 1)]->clear();
}

void AudioBridge::request_dac_idle() {
  playback_owner_.store(0);
  owner_handoff_at_ms_.store(0);
  dac_idle_requested_.store(true);
  routing_revision_.fetch_add(1, std::memory_order_relaxed);
  for (std::unique_ptr<FrameRing>& ring : rings_) {
    if (ring != nullptr) {
      ring->clear();
    }
  }
}

void AudioBridge::drop_and_close_dac_locked() {
  if (dac_handle_ != nullptr) {
    snd_pcm_drop(dac_handle_);
    snd_pcm_close(dac_handle_);
    dac_handle_ = nullptr;
  }
  dac_state_.store(DacLifecycleState::Idle);
}

bool AudioBridge::ensure_dac_open_locked() {
  if (dac_handle_ != nullptr) {
    return true;
  }
  if (active_config_.mode != ProfileMode::DacAssigned || !active_config_.dac_profile.has_value() ||
      active_config_.alsa_dac_device.empty()) {
    return false;
  }
  if (snd_pcm_open(&dac_handle_, active_config_.alsa_dac_device.c_str(), SND_PCM_STREAM_PLAYBACK, 0) < 0) {
    dac_state_.store(DacLifecycleState::Unavailable);
    return false;
  }
  std::string error;
  if (!configure_pcm(dac_handle_, *active_config_.dac_profile, config_.period_frames,
                     config_.buffer_frames, error)) {
    snd_pcm_close(dac_handle_);
    dac_handle_ = nullptr;
    dac_state_.store(DacLifecycleState::Unavailable);
    return false;
  }
  dac_state_.store(DacLifecycleState::Open);
  return true;
}

void AudioBridge::end_session_idle() {
  request_dac_idle();
  std::lock_guard lock(dac_mutex_);
  drop_and_close_dac_locked();
  dac_idle_requested_.store(false);
}

int AudioBridge::playback_owner() const { return playback_owner_.load(); }

int AudioBridge::handoff_source_zone() const { return handoff_source_zone_.load(); }

void AudioBridge::clear_zone_ring(int zone_id) {
  if (zone_id < 1 || zone_id > config_.zone_count) {
    return;
  }
  rings_[static_cast<size_t>(zone_id - 1)]->clear();
  last_buffered_at_ms_[zone_id].store(0);
}

void AudioBridge::set_playback_owner(int zone_id) {
  if (zone_id <= 0) {
    end_session_idle();
    return;
  }
  dac_idle_requested_.store(false);
  const int previous_owner = playback_owner_.load();
  {
    std::lock_guard lock(dac_mutex_);
    if (!ensure_dac_open_locked()) {
      if (previous_owner > 0) {
        playback_owner_.store(previous_owner);
      }
      return;
    }
  }
  if (previous_owner > 0 && previous_owner != zone_id) {
    handoff_source_zone_.store(previous_owner);
    owner_handoff_at_ms_.store(steady_now_ms());
  } else if (previous_owner != zone_id) {
    owner_handoff_at_ms_.store(steady_now_ms());
  }
  playback_owner_.store(zone_id);
  routing_revision_.fetch_add(1, std::memory_order_relaxed);
}

bool AudioBridge::zone_recently_buffered(int zone_id, int64_t within_ms) const {
  if (zone_id < 1 || zone_id > kMaxZones || within_ms <= 0) {
    return false;
  }
  const int64_t last_buffered_at = last_buffered_at_ms_[zone_id].load();
  if (last_buffered_at <= 0) {
    return false;
  }
  return steady_now_ms() - last_buffered_at <= within_ms;
}

size_t AudioBridge::zone_available_frames(int zone_id) const {
  if (zone_id < 1 || zone_id > config_.zone_count) {
    return 0;
  }
  return rings_[static_cast<size_t>(zone_id - 1)]->available_frames();
}

void AudioBridge::discard_stale_owner_frames(int owner, FrameRing& ring, uint8_t* buffer) {
  const int64_t last_buffered_at = last_buffered_at_ms_[owner].load();
  if (last_buffered_at <= 0 || steady_now_ms() - last_buffered_at <= kCaptureStaleMs) {
    return;
  }
  while (ring.available_frames() >= config_.period_frames) {
    ring.read(buffer, config_.period_frames);
  }
}

bool AudioBridge::playback_owner_unchanged(int owner, uint64_t revision) const {
  return owner > 0 && playback_owner_.load() == owner &&
         routing_revision_.load(std::memory_order_relaxed) == revision;
}

bool AudioBridge::prepare_playback_config(const ActiveAudioConfig& config) {
  active_config_ = config;
  bytes_per_frame_ = config.loopback_profile.channels *
                     (config.loopback_profile.sample_format == homepi::storage::SampleFormat::S32Le
                          ? sizeof(int32_t)
                          : sizeof(int16_t));

  for (int zone = 1; zone <= config_.zone_count; ++zone) {
    rings_[static_cast<size_t>(zone - 1)]->init(config_.period_frames * 8, bytes_per_frame_);
  }

  if (config.mode == ProfileMode::DacAssigned && config.dac_profile.has_value() &&
      !config.alsa_dac_device.empty()) {
    std::lock_guard lock(dac_mutex_);
    if (!ensure_dac_open_locked()) {
      bridge_state_.store(AudioBridgeState::Degraded);
      return true;
    }
  } else {
    dac_state_.store(DacLifecycleState::Unassigned);
  }
  return true;
}

void AudioBridge::close_all_captures() {
  for (int zone_id = 1; zone_id <= config_.zone_count; ++zone_id) {
    close_zone_capture_internal(zone_id);
  }
}

void AudioBridge::close_playback_device() {
  std::lock_guard lock(dac_mutex_);
  drop_and_close_dac_locked();
}

AudioBridge::StartResult AudioBridge::start(const ActiveAudioConfig& config) {
  StartResult result;
  if (config.status == ProfileStatus::PausedInvalid) {
    result.ok = false;
    result.error = "audio profile paused";
    result.dac_state = DacLifecycleState::Paused;
    bridge_state_.store(AudioBridgeState::Paused);
    return result;
  }

  stop_requested_.store(false);
  if (!prepare_playback_config(config)) {
    result.error = "failed to prepare audio config";
    result.dac_state = dac_state_.load();
    return result;
  }

  running_.store(true);
  bridge_state_.store(AudioBridgeState::Running);
  playback_thread_ = std::thread([this]() { playback_loop(); });
  result.ok = true;
  result.dac_state = dac_state_.load();
  return result;
}

bool AudioBridge::reload(const ActiveAudioConfig& config) {
  pause();
  return start(config).ok;
}

void AudioBridge::pause() {
  stop();
  bridge_state_.store(AudioBridgeState::Paused);
  dac_state_.store(DacLifecycleState::Paused);
}

void AudioBridge::stop() {
  stop_requested_.store(true);
  running_.store(false);
  close_all_captures();
  if (playback_thread_.joinable()) {
    playback_thread_.join();
  }
  close_playback_device();
  bridge_state_.store(AudioBridgeState::Stopped);
}

void AudioBridge::capture_loop(int zone_index) {
  const int zone_id = zone_index + 1;
  ZoneCaptureSlot& slot = capture_slots_[zone_id];
  snd_pcm_t* handle = nullptr;
  {
    std::lock_guard lock(slot.mutex);
    handle = slot.handle;
  }
  if (handle == nullptr) {
    slot.running.store(false);
    return;
  }

  std::vector<uint8_t> buffer(config_.period_frames * bytes_per_frame_);
  while (!stop_requested_.load() && !slot.stop_requested.load()) {
    const ZoneCaptureMode mode = zone_modes_[zone_id].load();
    if (mode == ZoneCaptureMode::Off || mode == ZoneCaptureMode::Disabled) {
      std::this_thread::sleep_for(std::chrono::milliseconds(10));
      continue;
    }
    const snd_pcm_sframes_t frames =
        snd_pcm_readi(handle, buffer.data(), config_.period_frames);
    if (frames < 0) {
      snd_pcm_recover(handle, static_cast<int>(frames), 1);
      stats_.capture_xruns++;
      continue;
    }
    if (mode == ZoneCaptureMode::Buffer) {
      if (rings_[zone_index]->write(buffer.data(), static_cast<size_t>(frames)) > 0) {
        last_buffered_at_ms_[zone_id].store(steady_now_ms());
      }
    }
  }

  std::lock_guard lock(slot.mutex);
  if (slot.handle != nullptr) {
    snd_pcm_drop(slot.handle);
    snd_pcm_close(slot.handle);
    slot.handle = nullptr;
  }
  slot.running.store(false);
}

void AudioBridge::playback_loop() {
  std::vector<uint8_t> buffer(config_.period_frames * bytes_per_frame_);
  while (!stop_requested_.load()) {
    if (dac_idle_requested_.exchange(false)) {
      std::lock_guard lock(dac_mutex_);
      drop_and_close_dac_locked();
    }
    {
      std::lock_guard lock(dac_mutex_);
      if (dac_handle_ == nullptr) {
        std::this_thread::sleep_for(std::chrono::milliseconds(5));
        continue;
      }
    }
    const int owner = playback_owner_.load();
    const uint64_t revision = routing_revision_.load(std::memory_order_relaxed);
    if (owner < 1 || owner > config_.zone_count) {
      std::this_thread::sleep_for(std::chrono::milliseconds(5));
      continue;
    }

    int read_zone = owner;
    const int handoff_zone = handoff_source_zone_.load();
    if (handoff_zone > 0 && handoff_zone <= config_.zone_count) {
      const bool target_ready =
          zone_available_frames(owner) >= config_.period_frames &&
          zone_recently_buffered(owner, kHandoffFreshBufferMs);
      if (!target_ready) {
        read_zone = handoff_zone;
      } else {
        handoff_source_zone_.store(0);
      }
    }

    FrameRing& ring = *rings_[read_zone - 1];
    discard_stale_owner_frames(read_zone, ring, buffer.data());
    const int64_t last_buffered_at = last_buffered_at_ms_[read_zone].load();
    const bool capture_stale =
        last_buffered_at > 0 && steady_now_ms() - last_buffered_at > kCaptureStaleMs;
    if (capture_stale) {
      std::this_thread::sleep_for(std::chrono::milliseconds(1));
      continue;
    }
    const int64_t handoff_at = owner_handoff_at_ms_.load();
    const int64_t now_ms = steady_now_ms();
    const bool recent_handoff = handoff_at > 0 && now_ms - handoff_at <= 2000;
    const auto deadline = std::chrono::steady_clock::now() +
                          std::chrono::milliseconds(recent_handoff ? 50 : 250);
    while (ring.available_frames() < config_.period_frames && !stop_requested_.load()) {
      if (playback_owner_.load() != owner ||
          routing_revision_.load(std::memory_order_relaxed) != revision) {
        break;
      }
      if (handoff_source_zone_.load() != handoff_zone) {
        break;
      }
      if (std::chrono::steady_clock::now() >= deadline) {
        break;
      }
      std::this_thread::sleep_for(std::chrono::milliseconds(1));
    }
    if (!playback_owner_unchanged(owner, revision)) {
      continue;
    }
    if (ring.available_frames() >= config_.period_frames &&
        ring.read(buffer.data(), config_.period_frames) >= config_.period_frames) {
      if (!playback_owner_unchanged(owner, revision)) {
        continue;
      }
      std::lock_guard lock(dac_mutex_);
      if (dac_handle_ == nullptr) {
        continue;
      }
      const snd_pcm_sframes_t written =
          snd_pcm_writei(dac_handle_, buffer.data(), config_.period_frames);
      if (written < 0) {
        snd_pcm_recover(dac_handle_, static_cast<int>(written), 1);
        stats_.playback_xruns++;
      } else {
        stats_.frames_copied += config_.period_frames;
        dac_state_.store(DacLifecycleState::Open);
      }
    } else {
      std::this_thread::sleep_for(std::chrono::milliseconds(1));
    }
  }
}

}  // namespace homepi::pcm_router
