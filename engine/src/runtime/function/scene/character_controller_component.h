#pragma once

#include "runtime/core/math/math_types.h"

#include <algorithm>
#include <cstdint>

namespace Blunder {

struct CharacterControllerComponent final {
  float radius{0.4f};
  float height{1.8f};
  float slope_limit_degrees{45.0f};
  float step_height{0.3f};
  float snap_length{0.2f};
  float skin{0.04f};
  uint32_t mask{0xFFFFFFFFu};
  Vec3 velocity{0.0f};
  bool on_floor{false};
  bool on_wall{false};
  bool on_ceiling{false};
};

inline void sanitizeCharacterControllerComponent(CharacterControllerComponent& cct) {
  cct.radius = std::max(cct.radius, 1e-4f);
  cct.height = std::max(cct.height, cct.radius * 2.0f);
  cct.slope_limit_degrees = std::clamp(cct.slope_limit_degrees, 0.0f, 89.0f);
  cct.step_height = std::max(cct.step_height, 0.0f);
  cct.snap_length = std::max(cct.snap_length, 0.0f);
  cct.skin = std::max(cct.skin, 0.0f);
}

inline bool characterControllerAuthoredEqual(const CharacterControllerComponent& a,
                                             const CharacterControllerComponent& b) {
  return a.radius == b.radius && a.height == b.height &&
         a.slope_limit_degrees == b.slope_limit_degrees && a.step_height == b.step_height &&
         a.snap_length == b.snap_length && a.skin == b.skin && a.mask == b.mask;
}

inline float characterControllerHalfHeight(const CharacterControllerComponent& cct) {
  return std::max(0.0f, cct.height * 0.5f - cct.radius);
}

}  // namespace Blunder
