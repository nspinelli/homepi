#include "homepi/metadata/metadata-parser.hpp"

#include <algorithm>
#include <cctype>
#include <cstring>
#include <iomanip>
#include <sstream>
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
constexpr std::uint32_t kCodePrgr = 0x70726772;  // "prgr"
constexpr std::uint32_t kCodePhbt = 0x70686274;  // "phbt"
constexpr std::uint32_t kCodePhb0 = 0x70686230;  // "phb0"
constexpr std::uint32_t kCodePict = 0x50494354;  // "PICT"
constexpr std::uint32_t kCodePbeg = 0x70626567;  // "pbeg"
constexpr std::uint32_t kCodePend = 0x70656e64;  // "pend"
constexpr std::uint32_t kCodePaus = 0x70617573;  // "paus"
constexpr std::uint32_t kCodePrsm = 0x7072736d;  // "prsm"
constexpr std::uint32_t kCodeAbeg = 0x61626567;  // "abeg"
constexpr std::uint32_t kCodeAend = 0x61656e64;  // "aend"
constexpr std::uint32_t kCodeMdst = 0x6d647374;  // "mdst"
constexpr std::uint32_t kCodeMden = 0x6d64656e;  // "mden"
constexpr std::uint32_t kCodeMper = 0x6d706572;  // "mper"

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
  progress_start_rtp_ = 0;
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
    return;
  }

  std::vector<std::uint8_t> payload;
  if (!extract_payload(item_xml, payload)) {
    return;
  }

  if (payload.size() < length) {
    return;
  }

  if (payload.size() > length) {
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
      std::ostringstream hex;
      hex << "0x" << std::hex << std::setfill('0');
      for (unsigned char byte : payload) {
        hex << std::setw(2) << static_cast<unsigned>(byte);
      }
      callbacks_.on_field({"track_id", hex.str()});
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

  if (code == kCodePrgr && callbacks_.on_progress) {
    const std::string progress(payload.begin(), payload.end());
    progress_start_rtp_ = parse_progress_start_rtp(progress);
    callbacks_.on_progress(parse_prgr_progress(progress, sample_rate_hz_));
    return;
  }

  if (code == kCodePhb0) {
    progress_start_rtp_ = parse_progress_start_rtp(std::string(payload.begin(), payload.end()));
    return;
  }

  if (code == kCodePhbt && callbacks_.on_progress) {
    callbacks_.on_progress(
        parse_phbt_progress(std::string(payload.begin(), payload.end()), sample_rate_hz_,
                            progress_start_rtp_));
    return;
  }

  if (code == kCodePict) {
    if (!payload.empty() && callbacks_.on_cover_art) {
      callbacks_.on_cover_art(payload);
    }
    return;
  }

  if (code == kCodePbeg && callbacks_.on_playback_state) {
    callbacks_.on_playback_state(true);
    return;
  }

  if ((code == kCodePend || code == kCodePaus) && callbacks_.on_playback_state) {
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

  const std::size_t padding =
      trimmed.size() >= 2 && trimmed[trimmed.size() - 1] == '='
          ? (trimmed[trimmed.size() - 2] == '=' ? 2 : 1)
          : 0;
  if (((trimmed.size() - padding) % 4) != 0) {
    return false;
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
