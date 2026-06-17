#pragma once

#include <string>

namespace homepi::events {

/** Validated HomePi event envelope fields. */
struct EventEnvelope {
  int version = 1;
  std::string id;
  std::string source;
  std::string topic;
  std::string event;
  std::string correlation_id;
  std::string timestamp;
  std::string payload_json;
};

/**
 * Returns an ISO8601 UTC timestamp with millisecond suffix.
 * @return Timestamp string.
 */
std::string iso_timestamp();

/**
 * Escapes a string for JSON output.
 * @param value Raw string.
 * @return Escaped JSON string content without surrounding quotes.
 */
std::string escape_json_string(const std::string& value);

/**
 * Builds a single-line NDJSON event envelope.
 * @param envelope Envelope fields.
 * @return JSON line without trailing newline.
 */
std::string build_event_line(const EventEnvelope& envelope);

}  // namespace homepi::events
