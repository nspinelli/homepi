#include "homepi/audio-paging/chime-player.hpp"

#include "homepi/audio-paging/pcm-player.hpp"

namespace homepi::audio_paging {

ChimePlayer::ChimePlayer(PcmPlayer* pcm_player) : pcm_player_(pcm_player) {}

bool ChimePlayer::play(const std::string& chime_file_path) {
  if (pcm_player_ == nullptr) {
    return false;
  }
  return pcm_player_->play_wav_file(chime_file_path);
}

}  // namespace homepi::audio_paging
