#pragma once

#include <cstdint>

namespace Blunder {

/// Generational handle: low 32 bits index, high 32 bits generation.
using ObjectId = uint64_t;

constexpr ObjectId k_invalid_object_id = 0u;

inline bool isValid(ObjectId id) { return id != k_invalid_object_id; }

inline uint32_t objectIdIndex(ObjectId id) {
  return static_cast<uint32_t>(id & 0xffffffffu);
}

inline uint32_t objectIdGeneration(ObjectId id) {
  return static_cast<uint32_t>(id >> 32);
}

inline ObjectId makeObjectId(uint32_t index, uint32_t generation) {
  return (static_cast<ObjectId>(generation) << 32) |
         static_cast<ObjectId>(index);
}

}  // namespace Blunder
