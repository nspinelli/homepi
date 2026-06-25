#include "homepi/metadata/cover-art-validator.hpp"

namespace homepi::metadata {

bool CoverArtValidator::is_valid_image(const std::vector<std::uint8_t>& bytes,
                                       std::size_t max_bytes) {
  if (bytes.empty() || bytes.size() > max_bytes) {
    return false;
  }
  if (bytes.size() >= 3 && bytes[0] == 0xff && bytes[1] == 0xd8 && bytes[2] == 0xff) {
    return true;
  }
  if (bytes.size() >= 8 && bytes[0] == 0x89 && bytes[1] == 0x50 && bytes[2] == 0x4e &&
      bytes[3] == 0x47 && bytes[4] == 0x0d && bytes[5] == 0x0a && bytes[6] == 0x1a &&
      bytes[7] == 0x0a) {
    return true;
  }
  return false;
}

}  // namespace homepi::metadata
