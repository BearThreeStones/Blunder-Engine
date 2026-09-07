#pragma once

#include <cstddef>
#include <cstdint>

namespace Blunder {

/// First-party SHA-256. out must have 32 bytes.
void sha256(const void* data, size_t size, uint8_t out[32]);

}  // namespace Blunder
