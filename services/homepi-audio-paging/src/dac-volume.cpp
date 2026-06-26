#include "homepi/audio-paging/dac-volume.hpp"

#include <array>
#include <cstdio>
#include <cstdlib>
#include <string>

namespace homepi::audio_paging {

namespace {

/**
 * Runs an amixer sset command for a named control on one card.
 * @param card_index - ALSA card number.
 * @param control_name - Mixer control name.
 * @param args - Remaining amixer arguments (for example `100% unmute`).
 * @returns True when amixer exits successfully.
 */
bool run_amixer_set(int card_index, const char* control_name, const char* args) {
  const std::string command = "amixer -c " + std::to_string(card_index) + " sset " + control_name +
                              " " + args + " >/dev/null 2>&1";
  return std::system(command.c_str()) == 0;
}

}  // namespace

std::optional<int> parse_alsa_card_index(const std::string& device_id) {
  const std::string marker = ":alsa:";
  const auto pos = device_id.rfind(marker);
  if (pos == std::string::npos) {
    return std::nullopt;
  }
  try {
    const int card_index = std::stoi(device_id.substr(pos + marker.size()));
    return card_index >= 0 ? std::optional<int>(card_index) : std::nullopt;
  } catch (...) {
    return std::nullopt;
  }
}

bool ensure_dac_playback_volume(int card_index) {
  if (card_index < 0) {
    return false;
  }
  if (run_amixer_set(card_index, "PCM", "100% unmute")) {
    return true;
  }
  return run_amixer_set(card_index, "Master", "100% unmute");
}

std::optional<int> read_dac_playback_volume_percent(int card_index) {
  if (card_index < 0) {
    return std::nullopt;
  }
  const std::string command = "amixer -c " + std::to_string(card_index) + " get PCM 2>/dev/null";
  std::array<char, 512> buffer{};
  FILE* pipe = popen(command.c_str(), "r");
  if (pipe == nullptr) {
    return std::nullopt;
  }
  std::string output;
  while (fgets(buffer.data(), static_cast<int>(buffer.size()), pipe) != nullptr) {
    output += buffer.data();
  }
  pclose(pipe);
  const auto percent_start = output.find('[');
  if (percent_start == std::string::npos) {
    return std::nullopt;
  }
  const auto percent_end = output.find('%', percent_start);
  if (percent_end == std::string::npos) {
    return std::nullopt;
  }
  try {
    return std::stoi(output.substr(percent_start + 1, percent_end - percent_start - 1));
  } catch (...) {
    return std::nullopt;
  }
}

}  // namespace homepi::audio_paging
