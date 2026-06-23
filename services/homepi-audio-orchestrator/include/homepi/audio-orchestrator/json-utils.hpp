#pragma once

#include <string>

namespace homepi::audio_orchestrator {

/**
 * Parses an integer field from a JSON object string.
 * @param json JSON object text.
 * @param field Field name.
 * @returns Parsed integer or 0 when missing.
 */
int parse_int_field(const std::string& json, const std::string& field);

/**
 * Parses a quoted string field from a JSON object string.
 * @param json JSON object text.
 * @param field Field name.
 * @returns Parsed string or empty when missing.
 */
std::string parse_string_field(const std::string& json, const std::string& field);

/**
 * Extracts the event name from an NDJSON envelope line.
 * @param line Broker event line.
 * @returns Event name or empty when missing.
 */
std::string parse_event_name(const std::string& line);

/**
 * Extracts the payload object from an NDJSON envelope line.
 * @param line Broker event line.
 * @returns Payload JSON object text.
 */
std::string parse_payload_json(const std::string& line);

}  // namespace homepi::audio_orchestrator
