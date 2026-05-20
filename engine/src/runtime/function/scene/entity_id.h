#pragma once

#include <cstdint>

namespace Blunder {

/// Dense entity handle within a SceneInstance (0 = invalid).
using EntityId = uint32_t;

constexpr EntityId k_invalid_entity_id = 0u;

inline bool isValid(EntityId id) { return id != k_invalid_entity_id; }

}  // namespace Blunder
