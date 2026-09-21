#pragma once

#include "core/math/fixed/fixed.h"

#include <cstdint>

namespace Blunder {

enum class MotionType { Dynamic, Static, Kinematic };

enum class ColliderShape { Box, Sphere, Capsule, TriangleMesh };

enum class PhysicsSweepShape { Ray, Box, Sphere, Capsule, TriangleMesh };

struct PhysicsTriangle {
  FixedVec3 v0{};
  FixedVec3 v1{};
  FixedVec3 v2{};
};

inline constexpr uint32_t kDefaultColliderLayer = 1u;
inline constexpr uint32_t kDefaultColliderMask = 0xFFFFFFFFu;

[[nodiscard]] inline Fixed fixedFromFloat(float value) {
  return Fixed::from_raw(static_cast<int64_t>(static_cast<double>(value) *
                                              static_cast<double>(Fixed::kOne)));
}

[[nodiscard]] inline float floatFromFixed(Fixed value) {
  return static_cast<float>(static_cast<double>(value.raw()) /
                            static_cast<double>(Fixed::kOne));
}

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

struct PhysicsQueryHit {
  bool hit = false;
  Fixed distance = Fixed::zero();
  FixedVec3 point{};
  FixedVec3 normal{};
  ColliderHandle collider{};
  RigidBodyHandle body{};
  uint64_t user_data = 0;
  bool is_area = false;
};

}  // namespace Blunder
