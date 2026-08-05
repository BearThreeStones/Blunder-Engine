#pragma once

#include "core/math/fixed/fixed.h"

#include <cstdint>

namespace Blunder {

enum class MotionType { Dynamic, Static, Kinematic };

enum class ColliderShape { Box, Sphere, Capsule };

struct PhysicsTransform {
  FixedVec3 position{};
  FixedQuat rotation{};
};

struct PhysicsMaterial {
  Fixed friction = Fixed::zero();
  Fixed restitution = Fixed::zero();
};

struct RigidBodyHandle {
  uint32_t index = UINT32_MAX;
  uint32_t generation = 0;

  [[nodiscard]] constexpr bool isValid() const { return index != UINT32_MAX; }
  friend constexpr bool operator==(RigidBodyHandle lhs, RigidBodyHandle rhs) {
    return lhs.index == rhs.index && lhs.generation == rhs.generation;
  }
  friend constexpr bool operator!=(RigidBodyHandle lhs, RigidBodyHandle rhs) { return !(lhs == rhs); }
};

struct ColliderHandle {
  uint32_t index = UINT32_MAX;
  uint32_t generation = 0;

  [[nodiscard]] constexpr bool isValid() const { return index != UINT32_MAX; }
  friend constexpr bool operator==(ColliderHandle lhs, ColliderHandle rhs) {
    return lhs.index == rhs.index && lhs.generation == rhs.generation;
  }
  friend constexpr bool operator!=(ColliderHandle lhs, ColliderHandle rhs) { return !(lhs == rhs); }
};

}  // namespace Blunder
