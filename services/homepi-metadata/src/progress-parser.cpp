#include "homepi/metadata/progress-parser.hpp"

#include <charconv>
#include <vector>

namespace homepi::metadata {

namespace {

long long parse_integer_token(std::string_view token) {
  long long value = 0;
  std::from_chars(token.data(), token.data() + token.size(), value);
  return value;
}

std::vector<std::string_view> split_tokens(std::string_view payload, char delimiter) {
  std::vector<std::string_view> tokens;
  std::size_t start = 0;
  while (start <= payload.size()) {
    const std::size_t end = payload.find(delimiter, start);
    if (end == std::string_view::npos) {
      tokens.push_back(payload.substr(start));
      break;
    }
    tokens.push_back(payload.substr(start, end - start));
    start = end + 1;
  }
  return tokens;
}

std::uint32_t read_be_u32(const std::vector<std::uint8_t>& payload) {
  if (payload.size() < 4) {
    return 0;
  }
  return (static_cast<std::uint32_t>(payload[0]) << 24) |
         (static_cast<std::uint32_t>(payload[1]) << 16) |
         (static_cast<std::uint32_t>(payload[2]) << 8) |
         static_cast<std::uint32_t>(payload[3]);
}

}  // namespace

int rtp_span_to_ms(long long start, long long end, int sample_rate_hz) {
  if (sample_rate_hz <= 0 || end < start) {
    return 0;
  }
  const long long samples = end - start;
  return static_cast<int>((samples * 1000LL) / sample_rate_hz);
}

MetadataProgressUpdate parse_prgr_progress(std::string_view payload, int sample_rate_hz) {
  MetadataProgressUpdate update;
  const auto tokens = split_tokens(payload, '/');
  if (tokens.size() != 3) {
    return update;
  }

  const long long start = parse_integer_token(tokens[0]);
  const long long current = parse_integer_token(tokens[1]);
  const long long end = parse_integer_token(tokens[2]);
  update.position_ms = rtp_span_to_ms(start, current, sample_rate_hz);
  update.duration_ms = rtp_span_to_ms(start, end, sample_rate_hz);
  update.has_position = true;
  update.has_duration = true;
  update.playing = true;
  return update;
}

MetadataProgressUpdate parse_phbt_progress(std::string_view payload, int sample_rate_hz,
                                           long long progress_start) {
  MetadataProgressUpdate update;
  const auto tokens = split_tokens(payload, '/');
  if (tokens.empty()) {
    return update;
  }

  const long long current = parse_integer_token(tokens[0]);
  if (progress_start > 0) {
    update.position_ms = rtp_span_to_ms(progress_start, current, sample_rate_hz);
    update.has_position = true;
    update.playing = true;
  }
  return update;
}

MetadataProgressUpdate parse_astm_duration(const std::vector<std::uint8_t>& payload) {
  MetadataProgressUpdate update;
  if (payload.size() < 4) {
    return update;
  }
  update.duration_ms = static_cast<int>(read_be_u32(payload));
  update.has_duration = true;
  update.playing = true;
  return update;
}

long long parse_progress_start_rtp(std::string_view payload) {
  const auto tokens = split_tokens(payload, '/');
  if (tokens.empty()) {
    return 0;
  }
  return parse_integer_token(tokens[0]);
}

}  // namespace homepi::metadata
