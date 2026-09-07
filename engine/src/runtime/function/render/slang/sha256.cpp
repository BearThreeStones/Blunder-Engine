#include "runtime/function/render/slang/sha256.h"

#include <cstring>

namespace Blunder {

namespace {

constexpr uint32_t k_sha256_k[64] = {
    0x428a2f98u, 0x71374491u, 0xb5c0fbcfu, 0xe9b5dba5u, 0x3956c25bu,
    0x59f111f1u, 0x923f82a4u, 0xab1c5ed5u, 0xd807aa98u, 0x12835b01u,
    0x243185beu, 0x550c7dc3u, 0x72be5d74u, 0x80deb1feu, 0x9bdc06a7u,
    0xc19bf174u, 0xe49b69c1u, 0xefbe4786u, 0x0fc19dc6u, 0x240ca1ccu,
    0x2de92c6fu, 0x4a7484aau, 0x5cb0a9dcu, 0x76f988dau, 0x983e5152u,
    0xa831c66du, 0xb00327c8u, 0xbf597fc7u, 0xc6e00bf3u, 0xd5a79147u,
    0x06ca6351u, 0x14292967u, 0x27b70a85u, 0x2e1b2138u, 0x4d2c6dfcu,
    0x53380d13u, 0x650a7354u, 0x766a0abbu, 0x81c2c92eu, 0x92722c85u,
    0xa2bfe8a1u, 0xa81a664bu, 0xc24b8b70u, 0xc76c51a3u, 0xd192e819u,
    0xd6990624u, 0xf40e3585u, 0x106aa070u, 0x19a4c116u, 0x1e376c08u,
    0x2748774cu, 0x34b0bcb5u, 0x391c0cb3u, 0x4ed8aa4au, 0x5b9cca4fu,
    0x682e6ff3u, 0x748f82eeu, 0x78a5636fu, 0x84c87814u, 0x8cc70208u,
    0x90befffau, 0xa4506cebu, 0xbef9a3f7u, 0xc67178f2u,
};

uint32_t rotr(uint32_t x, uint32_t n) { return (x >> n) | (x << (32u - n)); }

uint32_t loadBe32(const uint8_t* p) {
  return (static_cast<uint32_t>(p[0]) << 24u) |
         (static_cast<uint32_t>(p[1]) << 16u) |
         (static_cast<uint32_t>(p[2]) << 8u) | static_cast<uint32_t>(p[3]);
}

void storeBe32(uint8_t* p, uint32_t v) {
  p[0] = static_cast<uint8_t>(v >> 24u);
  p[1] = static_cast<uint8_t>(v >> 16u);
  p[2] = static_cast<uint8_t>(v >> 8u);
  p[3] = static_cast<uint8_t>(v);
}

void sha256Block(uint32_t state[8], const uint8_t block[64]) {
  uint32_t w[64];
  for (uint32_t i = 0; i < 16u; ++i) {
    w[i] = loadBe32(block + i * 4u);
  }
  for (uint32_t i = 16u; i < 64u; ++i) {
    const uint32_t s0 = rotr(w[i - 15u], 7u) ^ rotr(w[i - 15u], 18u) ^
                        (w[i - 15u] >> 3u);
    const uint32_t s1 = rotr(w[i - 2u], 17u) ^ rotr(w[i - 2u], 19u) ^
                        (w[i - 2u] >> 10u);
    w[i] = w[i - 16u] + s0 + w[i - 7u] + s1;
  }

  uint32_t a = state[0];
  uint32_t b = state[1];
  uint32_t c = state[2];
  uint32_t d = state[3];
  uint32_t e = state[4];
  uint32_t f = state[5];
  uint32_t g = state[6];
  uint32_t h = state[7];

  for (uint32_t i = 0; i < 64u; ++i) {
    const uint32_t s1 = rotr(e, 6u) ^ rotr(e, 11u) ^ rotr(e, 25u);
    const uint32_t ch = (e & f) ^ ((~e) & g);
    const uint32_t t1 = h + s1 + ch + k_sha256_k[i] + w[i];
    const uint32_t s0 = rotr(a, 2u) ^ rotr(a, 13u) ^ rotr(a, 22u);
    const uint32_t maj = (a & b) ^ (a & c) ^ (b & c);
    const uint32_t t2 = s0 + maj;
    h = g;
    g = f;
    f = e;
    e = d + t1;
    d = c;
    c = b;
    b = a;
    a = t1 + t2;
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

void sha256(const void* data, size_t size, uint8_t out[32]) {
  uint32_t state[8] = {0x6a09e667u, 0xbb67ae85u, 0x3c6ef372u, 0xa54ff53au,
                       0x510e527fu, 0x9b05688cu, 0x1f83d9abu, 0x5be0cd19u};

  const auto* bytes = static_cast<const uint8_t*>(data);
  size_t offset = 0;
  while (offset + 64u <= size) {
    sha256Block(state, bytes + offset);
    offset += 64u;
  }

  uint8_t last[64];
  std::memset(last, 0, sizeof(last));
  const size_t rem = size - offset;
  if (rem != 0) {
    std::memcpy(last, bytes + offset, rem);
  }
  last[rem] = 0x80u;

  const uint64_t bit_len = static_cast<uint64_t>(size) * 8u;
  if (rem >= 56u) {
    sha256Block(state, last);
    std::memset(last, 0, sizeof(last));
  }
  storeBe32(last + 56, static_cast<uint32_t>(bit_len >> 32u));
  storeBe32(last + 60, static_cast<uint32_t>(bit_len));
  sha256Block(state, last);

  for (uint32_t i = 0; i < 8u; ++i) {
    storeBe32(out + i * 4u, state[i]);
  }
}

}  // namespace Blunder
