#pragma once

#include <optional>
#include <string>
#include <vector>

#include "homepi/hifi-serial/types.hpp"

namespace homepi::hifi_serial {

/**
 * Parses a single Hi-Fi2 response line (without CR/LF).
 * @param line Response line starting with #.
 * @return Parsed updates (may be multiple for bulk responses).
 */
std::vector<ParsedUpdate> parse_response_line(const std::string& line);

/**
 * Extracts quoted string after position in line.
 * @param line Response line.
 * @param start Index after prefix.
 * @return Unquoted string or nullopt.
 */
std::optional<std::string> parse_quoted_value(const std::string& line, std::size_t start);

}  // namespace homepi::hifi_serial
