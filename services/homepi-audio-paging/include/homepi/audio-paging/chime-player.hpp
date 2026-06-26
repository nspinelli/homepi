#pragma once

#include <string>

namespace homepi::audio_paging {

class PcmPlayer;

/** Plays wav chimes on paging DAC through shared PCM player. */
class ChimePlayer {
 public:
  /** Creates chime player with existing PCM output dependency. */
  explicit ChimePlayer(PcmPlayer* pcm_player);

  /** Plays a configured chime wav file. */
  bool play(const std::string& chime_file_path);

 private:
  PcmPlayer* pcm_player_ = nullptr;
};

}  // namespace homepi::audio_paging
