#pragma once

#include <cstdint>
#include <string_view>
#include <vector>

namespace homepi::metadata {

/** Parsed Shairport metadata progress update. */
struct MetadataProgressUpdate {
  int position_ms = -1;
  int duration_ms = -1;
  bool playing = true;
  bool has_position = false;
  bool has_duration = false;
};

/**
 * Converts an RTP sample span to milliseconds.
 * @param start RTP sample at track start.
 * @param end RTP sample at current or track end.
 * @param sample_rate_hz Audio sample rate (44100 for AirPlay).
 * @returns Elapsed milliseconds, or 0 when invalid.
 */
int rtp_span_to_ms(long long start, long long end, int sample_rate_hz);

/**
 * Parses Shairport `prgr` progress payload (`start/current/end` RTP samples).
 * @param payload Progress string from MQTT or FIFO metadata.
 * @param sample_rate_hz Audio sample rate.
 * @returns Progress update with position and duration when parse succeeds.
 */
MetadataProgressUpdate parse_prgr_progress(std::string_view payload, int sample_rate_hz);

/**
 * Parses Shairport `phbt` frame position payload (`current/wall_clock_ns`).
 * @param payload Frame position string from MQTT.
 * @param sample_rate_hz Audio sample rate.
 * @param progress_start RTP sample captured from `phb0` / `prgr` start token.
 * @returns Progress update with position when start is known.
 */
MetadataProgressUpdate parse_phbt_progress(std::string_view payload, int sample_rate_hz,
                                           long long progress_start);

/**
 * Parses Shairport `astm` duration payload (4-byte big-endian milliseconds).
 * @param payload Raw duration bytes from MQTT or FIFO metadata.
 * @returns Progress update with duration when at least 4 bytes are present.
 */
MetadataProgressUpdate parse_astm_duration(const std::vector<std::uint8_t>& payload);

/**
 * Extracts the RTP start sample from a `prgr` or `phb0` payload.
 * @param payload Progress string.
 * @returns RTP start sample, or 0 when unavailable.
 */
long long parse_progress_start_rtp(std::string_view payload);

}  // namespace homepi::metadata
