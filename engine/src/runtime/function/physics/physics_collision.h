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
  const PhysicsTriangle* triangles = nullptr;
  uint32_t triangle_count = 0;
};

[[nodiscard]] ContactManifold collide(const ColliderWorldShape& a, const ColliderWorldShape& b);

[[nodiscard]] bool raycastShape(const ColliderWorldShape& shape, FixedVec3 origin,
                                FixedVec3 direction, Fixed max_distance, Fixed& out_t,
                                FixedVec3& out_point, FixedVec3& out_normal);

/// Sphere-swept mesh: offset faces plus edge capsules of `radius`. Used by capsule shapecast.
[[nodiscard]] bool raycastInflatedTriangleMesh(const ColliderWorldShape& shape, FixedVec3 origin,
                                               FixedVec3 direction, Fixed max_distance, Fixed radius,
                                               Fixed& out_t, FixedVec3& out_point,
                                               FixedVec3& out_normal);

}  // namespace Blunder
