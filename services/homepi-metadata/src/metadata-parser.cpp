#include "homepi/metadata/metadata-parser.hpp"

#include <algorithm>
#include <cctype>
#include <cstdint>
#include <cstring>
#include <string_view>

#include "homepi/metadata/progress-parser.hpp"

namespace homepi::metadata {

namespace {

constexpr std::uint32_t kTypeCore = 0x636f7265;  // "core"
constexpr std::uint32_t kTypeSsnc = 0x73736e63;  // "ssnc"

constexpr std::uint32_t kCodeMinm = 0x6d696e6d;  // "minm"
constexpr std::uint32_t kCodeAsar = 0x61736172;  // "asar"
constexpr std::uint32_t kCodeAsal = 0x6173616c;  // "asal"
constexpr std::uint32_t kCodeAstm = 0x6173746d;  // "astm"
constexpr std::uint32_t kCodeSnam = 0x736e616d;  // "snam"
constexpr std::uint32_t kCodeCmod = 0x636d6f64;  // "cmod"
constexpr std::uint32_t kCodeAscp = 0x61736370;  // "ascp"
constexpr std::uint32_t kCodeAsgn = 0x6173676e;  // "asgn"
constexpr std::uint32_t kCodePrgr = 0x70726772;  // "prgr"
constexpr std::uint32_t kCodePhbt = 0x70686274;  // "phbt"
constexpr std::uint32_t kCodePhb0 = 0x70686230;  // "phb0"
constexpr std::uint32_t kCodePict = 0x50494354;  // "PICT"
constexpr std::uint32_t kCodePcst = 0x70637374;  // "pcst"
constexpr std::uint32_t kCodePcen = 0x7063656e;  // "pcen"
constexpr std::uint32_t kCodeChnk = 0x63686e6b;  // "chnk"
constexpr std::uint32_t kCodePbeg = 0x70626567;  // "pbeg"
constexpr std::uint32_t kCodePend = 0x70656e64;  // "pend"
constexpr std::uint32_t kCodePaus = 0x70617573;  // "paus"
constexpr std::uint32_t kCodePrsm = 0x7072736d;  // "prsm"
constexpr std::uint32_t kCodeMper = 0x6d706572;  // "mper"
constexpr std::uint32_t kCodeAend = 0x61656e64;  // "aend"
constexpr std::uint32_t kCodeMdst = 0x6d647374;  // "mdst"
constexpr std::uint32_t kCodeMden = 0x6d64656e;  // "mden"
constexpr std::uint32_t kCodeAsul = 0x6173756c;  // "asul"
constexpr std::uint32_t kCodeAscm = 0x6173636d;  // "ascm"
constexpr std::uint32_t kCodeAsdt = 0x61736474;  // "asdt"
constexpr std::uint32_t kCodeAssn = 0x6173736e;  // "assn"
constexpr std::uint32_t kCodeSnua = 0x736e7561;  // "snua"
constexpr std::uint32_t kCodeClip = 0x636c6970;  // "clip"
constexpr std::uint32_t kCodeCdid = 0x63646964;  // "cdid"
constexpr std::uint32_t kCodeCmac = 0x636d6163;  // "cmac"

constexpr std::size_t kMaxPictureBytes = 2 * 1024 * 1024;

bool is_image_payload(const std::vector<std::uint8_t>& bytes) {
  if (bytes.size() >= 3 && bytes[0] == 0xff && bytes[1] == 0xd8 && bytes[2] == 0xff) {
    return true;
  }
  if (bytes.size() >= 8 && bytes[0] == 0x89 && bytes[1] == 0x50 && bytes[2] == 0x4e &&
      bytes[3] == 0x47) {
    return true;
  }
  return false;
}

bool parse_header(const std::string& item_xml, std::uint32_t& type, std::uint32_t& code,
                  std::size_t& length) {
  unsigned int type_hex = 0;
  unsigned int code_hex = 0;
  unsigned int length_value = 0;
  if (std::sscanf(item_xml.c_str(),
                  "<item><type>%8x</type><code>%8x</code><length>%u</length>",
                  &type_hex, &code_hex, &length_value) != 3) {
    return false;
  }
  type = type_hex;
  code = code_hex;
  length = length_value;
  return true;
}

}  // namespace

MetadataParser::MetadataParser(MetadataParserCallbacks callbacks)
    : callbacks_(std::move(callbacks)) {}

void MetadataParser::reset() {
  stream_buffer_.clear();
  pending_picture_.clear();
  picture_active_ = false;
  reset_progress_tracking();
}

void MetadataParser::reset_progress_tracking() {
  progress_start_rtp_ = 0;
  progress_end_rtp_ = 0;
  last_phbt_rtp_ = 0;
  phbt_accumulated_ms_ = 0;
}

void MetadataParser::feed(const char* data, std::size_t size, bool parse) {
  parse_enabled_ = parse;
  stream_buffer_.append(data, size);
  process_stream_buffer();
}

void MetadataParser::process_stream_buffer() {
  constexpr std::size_t kMaxBuffer = 8 * 1024 * 1024;

  while (true) {
    const std::size_t item_pos = stream_buffer_.find("<item>");
    if (item_pos == std::string::npos) {
      if (stream_buffer_.size() > kMaxBuffer) {
        stream_buffer_.clear();
      }
      return;
    }

    if (item_pos > 0) {
      stream_buffer_.erase(0, item_pos);
    }

    std::size_t body_end = std::string::npos;
    const std::size_t glued_data_item = stream_buffer_.find("</data><item>");
    const std::size_t close_item = stream_buffer_.find("</item>");
    const std::size_t next_item = stream_buffer_.find("<item>", 6);

    if (glued_data_item != std::string::npos &&
        (close_item == std::string::npos || glued_data_item < close_item)) {
      body_end = glued_data_item + std::strlen("</data>");
    } else if (close_item != std::string::npos) {
      body_end = close_item + std::strlen("</item>");
    } else if (next_item != std::string::npos) {
      const std::string header_probe = stream_buffer_.substr(0, next_item);
      if (header_probe.find("<length>0</length>") != std::string::npos) {
        body_end = next_item;
      } else if (header_probe.find("<data encoding=\"base64\">") == std::string::npos) {
        stream_buffer_.erase(0, next_item);
        continue;
      } else {
        return;
      }
    } else {
      return;
    }

    std::string item_xml = stream_buffer_.substr(0, body_end);
    stream_buffer_.erase(0, body_end);
    if (parse_enabled_) {
      process_item_xml(item_xml);
    }
  }
}

bool MetadataParser::extract_payload(const std::string& item_xml,
                                     std::vector<std::uint8_t>& payload) const {
  const auto data_tag = item_xml.find("<data encoding=\"base64\">");
  if (data_tag == std::string::npos) {
    return true;
  }

  const auto data_start = data_tag + std::strlen("<data encoding=\"base64\">");
  const auto data_end = item_xml.find("</data>", data_start);
  if (data_end == std::string::npos) {
    return false;
  }

  std::string encoded;
  encoded.reserve(data_end - data_start);
  for (char ch : item_xml.substr(data_start, data_end - data_start)) {
    if (ch != '\n' && ch != '\r') {
      encoded.push_back(ch);
    }
  }

  if (encoded.empty()) {
    payload.clear();
    return true;
  }

  return decode_base64(encoded, payload);
}

void MetadataParser::process_item_xml(const std::string& item_xml) {
  std::uint32_t type = 0;
  std::uint32_t code = 0;
  std::size_t length = 0;
  if (!parse_header(item_xml, type, code, length)) {
    return;
  }

  if (length == 0) {
    dispatch_item(type, code, {});
    return;
  }

  if (item_xml.find("<data encoding=\"base64\">") == std::string::npos) {
    const std::size_t nested = item_xml.find("<item>", 6);
    if (nested != std::string::npos) {
      process_item_xml(item_xml.substr(nested));
    }
    return;
  }

  std::vector<std::uint8_t> payload;
  if (!extract_payload(item_xml, payload)) {
    return;
  }

  // Shairport often declares a wire length larger than the decoded base64 payload.
  // Once the closing </item> is present, use whatever bytes were decoded.
  if (length > 0 && payload.empty()) {
    return;
  }

  if (payload.size() > length && length > 0) {
    payload.resize(length);
  }

  dispatch_item(type, code, payload);
}

void MetadataParser::dispatch_item(std::uint32_t type, std::uint32_t code,
                                   const std::vector<std::uint8_t>& payload) {
  if (type == kTypeSsnc && code == kCodeMdst && callbacks_.on_metadata_bundle_start) {
    callbacks_.on_metadata_bundle_start();
  }

  if (type == kTypeSsnc && code == kCodeMden && callbacks_.on_metadata_bundle_end) {
    callbacks_.on_metadata_bundle_end();
  }

  if (type == kTypeCore) {
    if (code == kCodeMinm && callbacks_.on_field) {
      callbacks_.on_field({"title", std::string(payload.begin(), payload.end())});
    } else if (code == kCodeAsar && callbacks_.on_field) {
      callbacks_.on_field({"artist", std::string(payload.begin(), payload.end())});
    } else if (code == kCodeAsal && callbacks_.on_field) {
      callbacks_.on_field({"album", std::string(payload.begin(), payload.end())});
    } else if (code == kCodeAstm && callbacks_.on_progress) {
      callbacks_.on_progress(parse_astm_duration(payload));
    } else if (code == kCodeMper && callbacks_.on_field) {
      const std::string persistent_id = parse_mper_persistent_id(payload);
      if (!persistent_id.empty()) {
        callbacks_.on_field({"persistent_id", persistent_id});
        callbacks_.on_field({"track_id", persistent_id});
      }
    } else if (code == kCodeAsul && callbacks_.on_field) {
      callbacks_.on_field({"stream_url", std::string(payload.begin(), payload.end())});
    } else if (code == kCodeAscm && callbacks_.on_field) {
      callbacks_.on_field({"comment", std::string(payload.begin(), payload.end())});
    } else if (code == kCodeAsdt && callbacks_.on_field) {
      callbacks_.on_field({"file_kind", std::string(payload.begin(), payload.end())});
    } else if (code == kCodeAssn && callbacks_.on_field) {
      callbacks_.on_field({"sort_title", std::string(payload.begin(), payload.end())});
    } else if (code == kCodeAscp && callbacks_.on_field) {
      callbacks_.on_field({"composer", std::string(payload.begin(), payload.end())});
    } else if (code == kCodeAsgn && callbacks_.on_field) {
      callbacks_.on_field({"genre", std::string(payload.begin(), payload.end())});
    }
    return;
  }

  if (type != kTypeSsnc) {
    return;
  }

  if (code == kCodeSnam && callbacks_.on_field) {
    callbacks_.on_field({"client_name", std::string(payload.begin(), payload.end())});
    return;
  }

  if (code == kCodeCmod && callbacks_.on_field) {
    callbacks_.on_field({"client_model", std::string(payload.begin(), payload.end())});
    return;
  }

  if (code == kCodeSnua && callbacks_.on_field) {
    callbacks_.on_field({"client_user_agent", std::string(payload.begin(), payload.end())});
    return;
  }

  if (code == kCodeClip && callbacks_.on_field) {
    callbacks_.on_field({"client_ip", std::string(payload.begin(), payload.end())});
    return;
  }

  if (code == kCodeCdid && callbacks_.on_field) {
    callbacks_.on_field({"client_device_id", std::string(payload.begin(), payload.end())});
    return;
  }

  if (code == kCodeCmac && callbacks_.on_field) {
    callbacks_.on_field({"client_mac", std::string(payload.begin(), payload.end())});
    return;
  }

  if (code == kCodePrgr && callbacks_.on_progress) {
    const std::string progress(payload.begin(), payload.end());
    progress_start_rtp_ = parse_progress_start_rtp(progress);
    progress_end_rtp_ = parse_progress_end_rtp(progress);
    last_phbt_rtp_ = 0;
    phbt_accumulated_ms_ = 0;
    callbacks_.on_progress(parse_prgr_progress(progress, sample_rate_hz_));
    return;
  }

  if (code == kCodePhb0) {
    progress_start_rtp_ = parse_progress_start_rtp(std::string(payload.begin(), payload.end()));
    last_phbt_rtp_ = 0;
    phbt_accumulated_ms_ = 0;
    return;
  }

  if (code == kCodePhbt && callbacks_.on_progress) {
    const std::string progress(payload.begin(), payload.end());
    if (progress_start_rtp_ <= 0) {
      progress_start_rtp_ = parse_progress_start_rtp(progress);
    }
    const long long current_value = parse_progress_current_rtp(progress);

    if (current_value > 1'000'000'000'000LL) {
      if (last_phbt_rtp_ > 0 && current_value > last_phbt_rtp_) {
        const long long delta_ns = current_value - last_phbt_rtp_;
        phbt_accumulated_ms_ += static_cast<int>(delta_ns / 1'000'000LL);
      }
      last_phbt_rtp_ = current_value;
    } else if (current_value > 0) {
      if (last_phbt_rtp_ > 0 && current_value > last_phbt_rtp_) {
        phbt_accumulated_ms_ +=
            rtp_span_to_ms(last_phbt_rtp_, current_value, sample_rate_hz_);
      } else if (progress_start_rtp_ > 0 && current_value > progress_start_rtp_) {
        phbt_accumulated_ms_ =
            rtp_span_to_ms(progress_start_rtp_, current_value, sample_rate_hz_);
      }
      last_phbt_rtp_ = current_value;
    }

    MetadataProgressUpdate update;
    update.position_ms = phbt_accumulated_ms_;
    update.playing = true;

    if (phbt_accumulated_ms_ <= 0 && progress_start_rtp_ > 0 && current_value > progress_start_rtp_) {
      const int rtp_position =
          rtp_span_to_ms(progress_start_rtp_, current_value, sample_rate_hz_);
      if (rtp_position > 0) {
        update.position_ms = rtp_position;
        phbt_accumulated_ms_ = rtp_position;
      }
    }

    if (progress_end_rtp_ > progress_start_rtp_) {
      const int track_duration =
          rtp_span_to_ms(progress_start_rtp_, progress_end_rtp_, sample_rate_hz_);
      if (track_duration > 0) {
        update.duration_ms = track_duration;
        update.has_duration = true;
      }
    }

    update.has_position = update.position_ms > 0 || last_phbt_rtp_ > 0;
    if (update.has_position) {
      callbacks_.on_progress(update);
    }
    return;
  }

  if (code == kCodePcst) {
    pending_picture_.clear();
    picture_active_ = true;
    return;
  }

  if (code == kCodeChnk && picture_active_) {
    if (!payload.empty() && pending_picture_.size() < kMaxPictureBytes) {
      const std::size_t remaining = kMaxPictureBytes - pending_picture_.size();
      const std::size_t copy_count = std::min(payload.size(), remaining);
      pending_picture_.insert(pending_picture_.end(), payload.begin(),
                              payload.begin() + static_cast<std::ptrdiff_t>(copy_count));
    }
    return;
  }

  if (code == kCodePict) {
    if (!payload.empty()) {
      if (picture_active_) {
        if (pending_picture_.size() < kMaxPictureBytes) {
          const std::size_t remaining = kMaxPictureBytes - pending_picture_.size();
          const std::size_t copy_count = std::min(payload.size(), remaining);
          pending_picture_.insert(pending_picture_.end(), payload.begin(),
                                  payload.begin() + static_cast<std::ptrdiff_t>(copy_count));
        }
        if (is_image_payload(pending_picture_) && callbacks_.on_cover_art) {
          callbacks_.on_cover_art(pending_picture_);
          pending_picture_.clear();
          picture_active_ = false;
        }
      } else if (is_image_payload(payload) && callbacks_.on_cover_art) {
        callbacks_.on_cover_art(payload);
      }
    }
    return;
  }

  if (code == kCodePcen) {
    if (picture_active_ && !pending_picture_.empty() && callbacks_.on_cover_art) {
      callbacks_.on_cover_art(pending_picture_);
    }
    pending_picture_.clear();
    picture_active_ = false;
    return;
  }

  if (code == kCodePbeg && callbacks_.on_playback_state) {
    callbacks_.on_playback_state(true);
    return;
  }

  if ((code == kCodePend || code == kCodePaus) && callbacks_.on_playback_state) {
    reset_progress_tracking();
    callbacks_.on_playback_state(false);
    if (code == kCodePend && callbacks_.on_session_cleared) {
      callbacks_.on_session_cleared();
    }
    return;
  }

  if (code == kCodePrsm && callbacks_.on_playback_state) {
    callbacks_.on_playback_state(true);
  }
}

bool MetadataParser::decode_base64(const std::string& encoded,
                                   std::vector<std::uint8_t>& out) const {
  static const int table[256] = {
      -1, -1, -1, -1, -1, -1, -1, -1, -1, -1, -1, -1, -1, -1, -1, -1, -1, -1, -1, -1, -1, -1,
      -1, -1, -1, -1, -1, -1, -1, -1, -1, -1, -1, -1, -1, -1, -1, -1, -1, -1, -1, -1, -1, 62,
      -1, -1, -1, 63, 52, 53, 54, 55, 56, 57, 58, 59, 60, 61, -1, -1, -1, -1, -1, -1, -1, 0,
      1,  2,  3,  4,  5,  6,  7,  8,  9,  10, 11, 12, 13, 14, 15, 16, 17, 18, 19, 20, 21, 22,
      23, 24, 25, -1, -1, -1, -1, -1, -1, 26, 27, 28, 29, 30, 31, 32, 33, 34, 35, 36, 37, 38,
      39, 40, 41, 42, 43, 44, 45, 46, 47, 48, 49, 50, 51, -1, -1, -1, -1, -1};

  std::string trimmed;
  trimmed.reserve(encoded.size());
  for (unsigned char ch : encoded) {
    if (std::isspace(ch) == 0) {
      trimmed.push_back(static_cast<char>(ch));
    }
  }

  if (trimmed.empty()) {
    out.clear();
    return true;
  }

  if ((trimmed.size() % 4) != 0) {
    trimmed.append(4 - (trimmed.size() % 4), '=');
  }

  out.clear();
  out.reserve((trimmed.size() / 4) * 3);
  std::uint32_t buffer = 0;
  int bits = 0;

  for (unsigned char ch : trimmed) {
    if (ch == '=') {
      break;
    }
    const int value = table[ch];
    if (value < 0) {
      return false;
    }
    buffer = (buffer << 6) | static_cast<std::uint32_t>(value);
    bits += 6;
    if (bits >= 8) {
      bits -= 8;
      out.push_back(static_cast<std::uint8_t>((buffer >> bits) & 0xFF));
    }
  }
  return true;
}

}  // namespace homepi::metadata
