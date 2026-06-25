#pragma once

#include <cstdint>
#include <string>
#include <vector>

namespace homepi::metadata {

/**
 * Computes SHA-256 of raw bytes and returns lowercase hex.
 * @param bytes Input bytes.
 * @returns 64-character hex digest.
 */
std::string sha256_hex(const std::vector<std::uint8_t>& bytes);

}  // namespace homepi::metadata
