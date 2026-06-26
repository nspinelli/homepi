#pragma once

#include <optional>
#include <string>

namespace homepi::audio_paging {

/**
 * Parses the ALSA card index from a HomePi USB device id suffix.
 * @param device_id - Device id ending with `:alsa:<card>`.
 * @returns Card index when present.
 */
std::optional<int> parse_alsa_card_index(const std::string& device_id);

/**
 * Sets paging DAC hardware PCM playback to 100% and unmuted.
 * Page loudness is controlled per-zone in HiFi page volume settings.
 * @param card_index - ALSA card number for the assigned paging DAC.
 * @returns True when mixer controls were updated successfully.
 */
bool ensure_dac_playback_volume(int card_index);

/**
 * Reads current PCM playback volume percent from the paging DAC mixer.
 * @param card_index - ALSA card number for the assigned paging DAC.
 * @returns Volume percent when readable.
 */
std::optional<int> read_dac_playback_volume_percent(int card_index);

}  // namespace homepi::audio_paging
