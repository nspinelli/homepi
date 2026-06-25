#pragma once

#include <cstdint>
#include <vector>

namespace homepi::metadata {

/**
 * Validates cover art bytes before caching (spec §11.3).
 */
class CoverArtValidator {
 public:
  /**
   * Returns whether bytes look like a supported JPEG or PNG image.
   * @param bytes Raw image bytes.
   * @param max_bytes Maximum allowed payload size.
   * @returns True when valid and within size limits.
   */
  static bool is_valid_image(const std::vector<std::uint8_t>& bytes, std::size_t max_bytes = 2 * 1024 * 1024);
};

}  // namespace homepi::metadata
