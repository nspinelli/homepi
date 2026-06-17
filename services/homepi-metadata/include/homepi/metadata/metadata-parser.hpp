#pragma once

#include <cstddef>
#include <cstdint>
#include <functional>
#include <string>
#include <vector>

#include "homepi/metadata/progress-parser.hpp"

namespace homepi::metadata {

/** Parsed Shairport metadata field update. */
struct MetadataFieldUpdate {
  std::string field;
  std::string value;
};

/** Callbacks invoked when owner-zone metadata is parsed. */
struct MetadataParserCallbacks {
  std::function<void(const MetadataFieldUpdate&)> on_field;
  std::function<void(const MetadataProgressUpdate&)> on_progress;
  std::function<void(bool playing)> on_playback_state;
  std::function<void(const std::vector<std::uint8_t>&)> on_cover_art;
  std::function<void()> on_metadata_bundle_start;
  std::function<void()> on_session_cleared;
};

/**
 * Incremental parser for one Shairport metadata FIFO stream.
 * Drains all input; only invokes callbacks when parse mode is enabled.
 */
class MetadataParser {
 public:
  explicit MetadataParser(MetadataParserCallbacks callbacks);

  /**
   * Feeds bytes from a metadata pipe.
   * @param data Incoming bytes.
   * @param size Byte count.
   * @param parse When false, payload bodies are drained without decoding.
   */
  void feed(const char* data, std::size_t size, bool parse);

  /** Resets parser state for a new session. */
  void reset();

 private:
  void process_stream_buffer();
  void process_item_xml(const std::string& item_xml);
  void dispatch_item(std::uint32_t type, std::uint32_t code, const std::vector<std::uint8_t>& payload);
  bool decode_base64(const std::string& encoded, std::vector<std::uint8_t>& out) const;
  bool extract_payload(const std::string& item_xml, std::vector<std::uint8_t>& payload) const;

  MetadataParserCallbacks callbacks_;
  std::string stream_buffer_;
  bool parse_enabled_ = true;
  int sample_rate_hz_ = 44100;
  long long progress_start_rtp_ = 0;
};

}  // namespace homepi::metadata
