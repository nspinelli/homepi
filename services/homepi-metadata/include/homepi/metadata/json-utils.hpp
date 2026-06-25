#pragma once

#include <string>
#include <vector>

namespace homepi::metadata {

/**
 * Extracts the event name from a core event envelope line.
 * @param line NDJSON event line.
 * @returns Event name or empty string.
 */
std::string parse_event_name(const std::string& line);

/**
 * Extracts the payload object JSON from a core event envelope line.
 * @param line NDJSON event line.
 * @returns Payload JSON object text or empty object.
 */
std::string parse_payload_json(const std::string& line);

/**
 * Parses an integer JSON field from a loosely structured object string.
 * @param json JSON object text.
 * @param field Field name without quotes.
 * @returns Parsed integer or 0 when missing.
 */
int parse_int_field(const std::string& json, const std::string& field);

/**
 * Parses an integer JSON array field from a loosely structured object string.
 * @param json JSON object text.
 * @param field Field name without quotes.
 * @returns Parsed integers or empty vector when missing.
 */
std::vector<int> parse_int_array_field(const std::string& json, const std::string& field);

/**
 * Escapes a string for JSON output.
 * @param value Raw string value.
 * @returns Escaped JSON string contents without surrounding quotes.
 */
std::string escape_json_string(const std::string& value);

}  // namespace homepi::metadata
