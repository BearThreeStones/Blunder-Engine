#pragma once

#include "function/physics/physics_types.h"

namespace Blunder {

struct ContactManifold {
  FixedVec3 normal{};
  Fixed penetration = Fixed::zero();
  FixedVec3 point_on_a{};
  bool valid = false;
};

struct ColliderWorldShape {
  ColliderShape shape = ColliderShape::Box;
  PhysicsTransform pose{};
  FixedVec3 box_half_extents{};
  Fixed sphere_radius = Fixed::zero();
  Fixed capsule_radius = Fixed::zero();
  Fixed capsule_half_height = Fixed::zero();
};

[[nodiscard]] ContactManifold collide(const ColliderWorldShape& a, const ColliderWorldShape& b);

}  // namespace Blunder
