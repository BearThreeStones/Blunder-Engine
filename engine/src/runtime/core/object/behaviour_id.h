#pragma once

#include <cstdint>

namespace Blunder {

using BehaviourId = uint64_t;

constexpr BehaviourId k_invalid_behaviour_id = 0u;

inline bool isValidBehaviourId(BehaviourId id) {
  return id != k_invalid_behaviour_id;
}

}  // namespace Blunder
