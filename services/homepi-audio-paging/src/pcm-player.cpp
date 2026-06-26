#include "homepi/audio-paging/dac-volume.hpp"
#include "homepi/audio-paging/pcm-player.hpp"

#include <cstdlib>

namespace homepi::audio_paging {

PcmPlayer::PcmPlayer(std::string alsa_device) : alsa_device_(std::move(alsa_device)) {}

void PcmPlayer::set_alsa_card_index(int card_index) { alsa_card_index_ = card_index; }

bool PcmPlayer::run_aplay(const std::string& command) {
  if (alsa_card_index_ >= 0) {
    ensure_dac_playback_volume(alsa_card_index_);
  }
  open_ = true;
  if (on_open_) {
    on_open_();
  }
  const int rc = std::system(command.c_str());
  open_ = false;
  if (on_close_) {
    on_close_();
  }
  return rc == 0;
}

bool PcmPlayer::play_raw_file(const std::string& raw_path, int sample_rate_hz) {
  const std::string command = "aplay -q -D \"" + alsa_device_ + "\" -t raw -f S16_LE -r " +
                              std::to_string(sample_rate_hz) + " -c 1 \"" + raw_path + "\"";
  return run_aplay(command);
}

bool PcmPlayer::play_wav_file(const std::string& wav_path) {
  const std::string command = "aplay -q -D \"" + alsa_device_ + "\" \"" + wav_path + "\"";
  return run_aplay(command);
}

bool PcmPlayer::is_open() const { return open_; }

void PcmPlayer::set_state_callbacks(std::function<void()> on_open, std::function<void()> on_close) {
  on_open_ = std::move(on_open);
  on_close_ = std::move(on_close);
}

}  // namespace homepi::audio_paging
