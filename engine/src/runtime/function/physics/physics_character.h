#pragma once

#include "function/physics/physics_world.h"

namespace Blunder {

struct PhysicsCharacterMove {
  PhysicsTransform pose{};
  FixedVec3 displacement{};
  PhysicsSweepShape shape = PhysicsSweepShape::Capsule;
  Fixed radius = Fixed::from_int(1) / Fixed::from_int(2);
  Fixed half_height = Fixed::from_int(1) / Fixed::from_int(2);
  Fixed snap_length = Fixed::from_int(1) / Fixed::from_int(5);
  Fixed skin = Fixed::from_int(1) / Fixed::from_int(25);
  uint32_t mask = kDefaultColliderMask;
};

struct PhysicsCharacterResult {
  PhysicsTransform pose{};
  bool on_floor = false;
  bool on_wall = false;
  bool on_ceiling = false;
};

[[nodiscard]] PhysicsCharacterResult moveAndSlide(const PhysicsWorld& world,
                                                  const PhysicsCharacterMove& move);

}  // namespace Blunder
