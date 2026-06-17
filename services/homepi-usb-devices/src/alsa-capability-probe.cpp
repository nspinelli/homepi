#include "homepi/usb-devices/alsa-capability-probe.hpp"

#include <alsa/asoundlib.h>
#include <chrono>
#include <ctime>
#include <set>
#include <tuple>
#include <vector>

namespace homepi::usb_devices {

namespace {

std::string utc_now() {
  const auto now = std::chrono::system_clock::now();
  const std::time_t t = std::chrono::system_clock::to_time_t(now);
  std::tm tm{};
  gmtime_r(&t, &tm);
  char buffer[32];
  std::strftime(buffer, sizeof(buffer), "%Y-%m-%dT%H:%M:%SZ", &tm);
  return buffer;
}

bool try_tuple(snd_pcm_t* handle, snd_pcm_hw_params_t* params, snd_pcm_format_t format,
               unsigned int rate, unsigned int channels) {
  snd_pcm_hw_params_any(handle, params);
  if (snd_pcm_hw_params_set_access(handle, params, SND_PCM_ACCESS_RW_INTERLEAVED) < 0) {
    return false;
  }
  if (snd_pcm_hw_params_set_format(handle, params, format) < 0) {
    return false;
  }
  unsigned int requested_rate = rate;
  if (snd_pcm_hw_params_set_rate_near(handle, params, &requested_rate, nullptr) < 0) {
    return false;
  }
  if (requested_rate != rate) {
    return false;
  }
  if (snd_pcm_hw_params_set_channels(handle, params, channels) < 0) {
    return false;
  }
  return snd_pcm_hw_params(handle, params) >= 0;
}

}  // namespace

std::optional<homepi::storage::AudioCapabilities> AlsaCapabilityProbe::probe_playback(
    const std::string& device_id, const std::string& alsa_hw_name) const {
  snd_pcm_t* handle = nullptr;
  if (snd_pcm_open(&handle, alsa_hw_name.c_str(), SND_PCM_STREAM_PLAYBACK, 0) < 0) {
    return std::nullopt;
  }

  snd_pcm_hw_params_t* params = nullptr;
  snd_pcm_hw_params_alloca(&params);

  const std::vector<unsigned int> rates = {44100, 48000, 88200, 96000, 176400, 192000};
  const std::vector<std::pair<snd_pcm_format_t, homepi::storage::SampleFormat>> formats = {
      {SND_PCM_FORMAT_S16_LE, homepi::storage::SampleFormat::S16Le},
      {SND_PCM_FORMAT_S32_LE, homepi::storage::SampleFormat::S32Le},
  };
  const std::vector<unsigned int> channels = {1, 2};

  std::set<std::tuple<unsigned int, unsigned int, homepi::storage::SampleFormat>> seen;
  homepi::storage::AudioCapabilities capabilities;
  capabilities.device_id = device_id;
  capabilities.probed_at = utc_now();

  for (const auto& [alsa_format, storage_format] : formats) {
    for (unsigned int rate : rates) {
      for (unsigned int channel_count : channels) {
        if (!try_tuple(handle, params, alsa_format, rate, channel_count)) {
          continue;
        }
        const auto key = std::make_tuple(rate, channel_count, storage_format);
        if (!seen.insert(key).second) {
          continue;
        }
        homepi::storage::AudioProfileTuple tuple;
        tuple.sample_rate = rate;
        tuple.channels = static_cast<uint16_t>(channel_count);
        tuple.sample_format = storage_format;
        capabilities.supported_profile_tuples.push_back(tuple);
      }
    }
  }

  snd_pcm_close(handle);
  if (capabilities.supported_profile_tuples.empty()) {
    capabilities.probe_error = "No supported PCM profile tuples found";
    return capabilities;
  }
  return capabilities;
}

}  // namespace homepi::usb_devices
