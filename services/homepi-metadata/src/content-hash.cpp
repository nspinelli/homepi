#include "homepi/metadata/content-hash.hpp"

#include <array>
#include <cstdint>
#include <iomanip>
#include <sstream>
#include <vector>

namespace homepi::metadata {

namespace {

constexpr std::uint32_t rotr(std::uint32_t value, std::uint32_t bits) {
  return (value >> bits) | (value << (32U - bits));
}

void sha256_transform(std::array<std::uint32_t, 8>& state, const std::uint8_t block[64]) {
  static constexpr std::uint32_t k[64] = {
      0x428a2f98U, 0x71374491U, 0xb5c0fbcfU, 0xe9b5dba5U, 0x3956c25bU, 0x59f111f1U, 0x923f82a4U,
      0xab1c5ed5U, 0xd807aa98U, 0x12835b01U, 0x243185beU, 0x550c7dc3U, 0x72be5d74U, 0x80deb1feU,
      0x9bdc06a7U, 0xc19bf174U, 0xe49b69c1U, 0xefbe4786U, 0x0fc19dc6U, 0x240ca1ccU, 0x2de92c6fU,
      0x4a7484aaU, 0x5cb0a9dcU, 0x76f988daU, 0x983e5152U, 0xa831c66dU, 0xb00327c8U, 0xbf597fc7U,
      0xc6e00bf3U, 0xd5a79147U, 0x06ca6351U, 0x14292967U, 0x27b70a85U, 0x2e1b2138U, 0x4d2c6dfcU,
      0x53380d13U, 0x650a7354U, 0x766a0abbU, 0x81c2c92eU, 0x92722c85U, 0xa2bfe8a1U, 0xa81a664bU,
      0xc24b8b70U, 0xc76c51a3U, 0xd192e819U, 0xd6990624U, 0xf40e3585U, 0x106aa070U, 0x19a4c116U,
      0x1e376c08U, 0x2748774cU, 0x34b0bcb5U, 0x391c0cb3U, 0x4ed8aa4aU, 0x5b9cca4fU, 0x682e6ff3U,
      0x748f82eeU, 0x78a5636fU, 0x84c87814U, 0x8cc70208U, 0x90befffaU, 0xa4506cebU, 0xbef9a3f7U,
      0xc67178f2U};

  std::array<std::uint32_t, 64> w{};
  for (int i = 0; i < 16; ++i) {
    w[static_cast<std::size_t>(i)] =
        (static_cast<std::uint32_t>(block[i * 4]) << 24) |
        (static_cast<std::uint32_t>(block[i * 4 + 1]) << 16) |
        (static_cast<std::uint32_t>(block[i * 4 + 2]) << 8) |
        static_cast<std::uint32_t>(block[i * 4 + 3]);
  }
  for (int i = 16; i < 64; ++i) {
    const std::uint32_t s0 = rotr(w[static_cast<std::size_t>(i - 15)], 7) ^
                             rotr(w[static_cast<std::size_t>(i - 15)], 18) ^
                             (w[static_cast<std::size_t>(i - 15)] >> 3);
    const std::uint32_t s1 = rotr(w[static_cast<std::size_t>(i - 2)], 17) ^
                             rotr(w[static_cast<std::size_t>(i - 2)], 19) ^
                             (w[static_cast<std::size_t>(i - 2)] >> 10);
    w[static_cast<std::size_t>(i)] =
        w[static_cast<std::size_t>(i - 16)] + s0 + w[static_cast<std::size_t>(i - 7)] + s1;
  }

  std::uint32_t a = state[0];
  std::uint32_t b = state[1];
  std::uint32_t c = state[2];
  std::uint32_t d = state[3];
  std::uint32_t e = state[4];
  std::uint32_t f = state[5];
  std::uint32_t g = state[6];
  std::uint32_t h = state[7];

  for (int i = 0; i < 64; ++i) {
    const std::uint32_t s1 = rotr(e, 6) ^ rotr(e, 11) ^ rotr(e, 25);
    const std::uint32_t ch = (e & f) ^ ((~e) & g);
    const std::uint32_t temp1 = h + s1 + ch + k[i] + w[static_cast<std::size_t>(i)];
    const std::uint32_t s0 = rotr(a, 2) ^ rotr(a, 13) ^ rotr(a, 22);
    const std::uint32_t maj = (a & b) ^ (a & c) ^ (b & c);
    const std::uint32_t temp2 = s0 + maj;

    h = g;
    g = f;
    f = e;
    e = d + temp1;
    d = c;
    c = b;
    b = a;
    a = temp1 + temp2;
  }

  state[0] += a;
  state[1] += b;
  state[2] += c;
  state[3] += d;
  state[4] += e;
  state[5] += f;
  state[6] += g;
  state[7] += h;
}

}  // namespace

std::string sha256_hex(const std::vector<std::uint8_t>& bytes) {
  std::array<std::uint32_t, 8> state = {0x6a09e667U, 0xbb67ae85U, 0x3c6ef372U, 0xa54ff53aU,
                                        0x510e527fU, 0x9b05688cU, 0x1f83d9abU, 0x5be0cd19U};

  const std::uint64_t bit_len = static_cast<std::uint64_t>(bytes.size()) * 8ULL;
  std::vector<std::uint8_t> padded = bytes;
  padded.push_back(0x80);
  while ((padded.size() % 64) != 56) {
    padded.push_back(0x00);
  }
  for (int i = 7; i >= 0; --i) {
    padded.push_back(static_cast<std::uint8_t>((bit_len >> (i * 8)) & 0xFF));
  }

  for (std::size_t offset = 0; offset < padded.size(); offset += 64) {
    sha256_transform(state, padded.data() + offset);
  }

  std::ostringstream out;
  out << std::hex << std::setfill('0');
  for (const std::uint32_t word : state) {
    out << std::setw(8) << word;
  }
  return out.str();
}

}  // namespace homepi::metadata
