#pragma once

#include "EASTL/string.h"

#include "runtime/core/math/math_types.h"

#include <algorithm>
#include <cstdint>

namespace Blunder {

enum class CharacterControllerShapeKind : uint8_t {
  Capsule = 0,
  Sphere = 1,
};

struct CharacterControllerComponent final {
  CharacterControllerShapeKind shape{CharacterControllerShapeKind::Capsule};
  float radius{0.4f};
  float height{1.8f};
  /// Local-space offset of the sweep shape from the entity origin (metres).
  /// Godot Chocomel CollisionShape (0, 0.75, -0.24) Y-up → engine (0, 0.24, 0.75).
  Vec3 shape_offset{0.0f};
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
  if (cct.shape == CharacterControllerShapeKind::Sphere) {
    cct.height = std::max(cct.height, cct.radius * 2.0f);
  } else {
    cct.height = std::max(cct.height, cct.radius * 2.0f);
  }
  cct.slope_limit_degrees = std::clamp(cct.slope_limit_degrees, 0.0f, 89.0f);
  cct.step_height = std::max(cct.step_height, 0.0f);
  cct.snap_length = std::max(cct.snap_length, 0.0f);
  cct.skin = std::max(cct.skin, 0.0f);
}

inline bool characterControllerAuthoredEqual(const CharacterControllerComponent& a,
                                             const CharacterControllerComponent& b) {
  return a.shape == b.shape && a.radius == b.radius && a.height == b.height &&
         a.shape_offset == b.shape_offset &&
         a.slope_limit_degrees == b.slope_limit_degrees && a.step_height == b.step_height &&
         a.snap_length == b.snap_length && a.skin == b.skin && a.mask == b.mask;
}

inline float characterControllerHalfHeight(const CharacterControllerComponent& cct) {
  if (cct.shape == CharacterControllerShapeKind::Sphere) {
    return 0.0f;
  }
  return std::max(0.0f, cct.height * 0.5f - cct.radius);
}

inline const char* characterControllerShapeKindJson(CharacterControllerShapeKind shape) {
  switch (shape) {
    case CharacterControllerShapeKind::Sphere:
      return "sphere";
    case CharacterControllerShapeKind::Capsule:
    default:
      return "capsule";
  }
}

inline bool characterControllerShapeKindFromJson(const eastl::string& text,
                                                 CharacterControllerShapeKind& out) {
  if (text == "sphere") {
    out = CharacterControllerShapeKind::Sphere;
    return true;
  }
  if (text == "capsule") {
    out = CharacterControllerShapeKind::Capsule;
    return true;
  }
  return false;
}

}  // namespace Blunder
