#pragma once

#include <string>
#include <string_view>

#include "homepi/audio-paging/types.hpp"

namespace homepi::audio_paging {

/** Escapes a string for safe JSON embedding. */
std::string json_escape(std::string_view value);

/** Reads a JSON string field from loosely structured JSON text. */
std::string json_get_string(std::string_view json, std::string_view field);

/** Reads a JSON number/bool/string field as scalar text from JSON text. */
std::string json_get_scalar(std::string_view json, std::string_view field);

/** Reads a JSON object field and returns the object text including braces. */
std::string json_get_object(std::string_view json, std::string_view field);

/** Reads a JSON field from an event envelope payload object. */
std::string parse_payload_json(const std::string& event_line);

/** Reads the event name from an event envelope. */
std::string parse_event_name(const std::string& event_line);

/** Reads topic name from an event envelope. */
std::string parse_topic_name(const std::string& event_line);

/** Reads correlation id from an event envelope. */
std::string parse_correlation_id(const std::string& event_line);

/** Converts idle policy enum to API string. */
std::string idle_policy_to_string(PagingIdlePolicy policy);

/** Parses idle policy string and returns default when invalid. */
PagingIdlePolicy parse_idle_policy(const std::string& value, PagingIdlePolicy fallback);

/** Converts resource lifecycle state enum to API string. */
std::string resource_state_to_string(ResourceState state);

/** Builds compact JSON from paging status snapshot. */
std::string paging_status_to_json(const PagingStatus& status);

}  // namespace homepi::audio_paging
