#pragma once

#include <cstdlib>
#include <string>
#include <vector>

namespace homepi::pcm_router {

/**
 * Parses an integer JSON field from a loosely structured object string.
 * @param json - JSON object text.
 * @param field - Field name without quotes.
 * @returns Parsed integer or 0 when missing.
 */
int parse_int_field(const std::string& json, const std::string& field);

/**
 * Parses a boolean JSON field from a loosely structured object string.
 * @param json - JSON object text.
 * @param field - Field name without quotes.
 * @returns Parsed boolean or false when missing.
 */
bool parse_bool_field(const std::string& json, const std::string& field);

/**
 * Parses an integer array JSON field.
 * @param json - JSON object text.
 * @param field - Field name without quotes.
 * @returns Parsed integers.
 */
std::vector<int> parse_int_array(const std::string& json, const std::string& field);

/**
 * Extracts the event name from a core event envelope line.
 * @param line - NDJSON event line.
 * @returns Event name or empty string.
 */
std::string parse_event_name(const std::string& line);

/**
 * Extracts the payload object JSON from a core event envelope line.
 * @param line - NDJSON event line.
 * @returns Payload JSON object text or empty object.
 */
std::string parse_payload_json(const std::string& line);

}  // namespace homepi::pcm_router
