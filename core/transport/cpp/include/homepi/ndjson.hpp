#pragma once

#include <sstream>
#include <string>
#include <string_view>
#include <vector>

namespace homepi::transport {

/**
 * Splits accumulated NDJSON buffer into complete lines.
 */
inline std::pair<std::vector<std::string>, std::string> split_ndjson_lines(
    std::string_view buffer) {
  std::vector<std::string> complete;
  std::string remainder;
  std::istringstream stream{std::string(buffer)};
  std::string line;
  while (std::getline(stream, line)) {
    if (!stream.eof()) {
      if (!line.empty()) {
        complete.push_back(line);
      }
    } else {
      remainder = line;
    }
  }
  return {complete, remainder};
}

}  // namespace homepi::transport
