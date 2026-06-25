#include "homepi/metadata/progress-parser.hpp"

#include <charconv>
#include <cinttypes>
#include <cstdio>
#include <string>
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
  const long long ms = (samples * 1000LL) / sample_rate_hz;
  constexpr long long kMaxMs = 8LL * 60 * 60 * 1000;
  if (ms > kMaxMs) {
    return 0;
  }
  return static_cast<int>(ms);
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
  if (start == 0 && current == 0 && end == 0) {
    return update;
  }
  if (current >= start) {
    update.position_ms = rtp_span_to_ms(start, current, sample_rate_hz);
    update.has_position = true;
    update.playing = true;
  }
  if (end > start) {
    update.duration_ms = rtp_span_to_ms(start, end, sample_rate_hz);
    if (update.duration_ms > 0) {
      update.has_duration = true;
      update.playing = true;
    }
  }
  return update;
}

MetadataProgressUpdate parse_phbt_progress(std::string_view payload, int sample_rate_hz,
                                           long long progress_start) {
  MetadataProgressUpdate update;
  const auto tokens = split_tokens(payload, '/');
  if (tokens.empty()) {
    return update;
  }

  const long long current =
      tokens.size() >= 2 ? parse_integer_token(tokens[1]) : parse_integer_token(tokens[0]);
  if (progress_start > 0) {
    update.position_ms = rtp_span_to_ms(progress_start, current, sample_rate_hz);
    update.has_position = update.position_ms > 0;
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
  if (update.duration_ms > 0) {
    update.has_duration = true;
    update.playing = true;
  }
  return update;
}

long long parse_progress_start_rtp(std::string_view payload) {
  const auto tokens = split_tokens(payload, '/');
  if (tokens.empty()) {
    return 0;
  }
  return parse_integer_token(tokens[0]);
}

long long parse_progress_current_rtp(std::string_view payload) {
  const auto tokens = split_tokens(payload, '/');
  if (tokens.empty()) {
    return 0;
  }
  if (tokens.size() >= 2) {
    return parse_integer_token(tokens[1]);
  }
  return parse_integer_token(tokens[0]);
}

long long parse_progress_end_rtp(std::string_view payload) {
  const auto tokens = split_tokens(payload, '/');
  if (tokens.size() != 3) {
    return 0;
  }
  return parse_integer_token(tokens[2]);
}

std::string parse_mper_persistent_id(const std::vector<std::uint8_t>& payload) {
  if (payload.size() < 8) {
    return {};
  }
  const std::uint64_t high = read_be_u32(payload);
  std::uint32_t low_parts[1] = {0};
  if (payload.size() >= 8) {
    low_parts[0] = (static_cast<std::uint32_t>(payload[4]) << 24) |
                   (static_cast<std::uint32_t>(payload[5]) << 16) |
                   (static_cast<std::uint32_t>(payload[6]) << 8) |
                   static_cast<std::uint32_t>(payload[7]);
  }
  const std::uint64_t persistent_id = (high << 32) | static_cast<std::uint64_t>(low_parts[0]);
  char buffer[32];
  std::snprintf(buffer, sizeof(buffer), "0x%" PRIx64, persistent_id);
  return buffer;
}

}  // namespace homepi::metadata
