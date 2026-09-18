#include "function/physics/physics_character.h"

namespace Blunder {
namespace {

FixedVec3 worldUp() { return FixedVec3(Fixed::zero(), Fixed::zero(), Fixed::from_int(1)); }

Fixed walkableDot() { return Fixed::from_int(7) / Fixed::from_int(10); }

FixedVec3 slideAlong(FixedVec3 remaining, FixedVec3 normal) {
  const Fixed n = dot(remaining, normal);
  if (n.raw() >= 0) {
    return remaining;
  }
  return remaining - normal * n;
}

}  // namespace

PhysicsCharacterResult moveAndSlide(const PhysicsWorld& world, const PhysicsCharacterMove& move) {
  PhysicsCharacterResult result{};
  result.pose = move.pose;

  FixedVec3 remaining = move.displacement;
  const Fixed remaining_len_sq_start = dot(remaining, remaining);
  if (remaining_len_sq_start.raw() == 0) {
    remaining = FixedVec3{};
  }

  for (int iter = 0; iter < 4; ++iter) {
    const Fixed len_sq = dot(remaining, remaining);
    if (len_sq.raw() == 0) {
      break;
    }
    const Fixed length = sqrt(len_sq);
    const FixedVec3 dir = remaining / length;
    PhysicsQueryHit hit{};
    const bool blocked =
        world.shapecast(PhysicsSweepShape::Capsule, result.pose, FixedVec3{}, Fixed::zero(),
                        move.radius, move.half_height, dir, length, move.mask, false, hit);
    if (!blocked) {
      result.pose.position = result.pose.position + remaining;
      break;
    }

    Fixed travel = hit.distance - move.skin;
    if (travel.raw() < 0) {
      travel = Fixed::zero();
    }
    result.pose.position = result.pose.position + dir * travel;
    remaining = remaining - dir * travel;
    remaining = slideAlong(remaining, hit.normal);

    const Fixed up_dot = dot(hit.normal, worldUp());
    if (up_dot.raw() >= walkableDot().raw()) {
      result.on_floor = true;
    } else if (up_dot.raw() <= -walkableDot().raw()) {
      result.on_ceiling = true;
    } else {
      result.on_wall = true;
    }
  }

  if (move.snap_length.raw() > 0) {
    PhysicsQueryHit floor{};
    const FixedVec3 down(Fixed::zero(), Fixed::zero(), Fixed::from_int(-1));
    if (world.shapecast(PhysicsSweepShape::Capsule, result.pose, FixedVec3{}, Fixed::zero(),
                        move.radius, move.half_height, down, move.snap_length, move.mask, false,
                        floor)) {
      const Fixed up_dot = dot(floor.normal, worldUp());
      if (up_dot.raw() >= walkableDot().raw()) {
        Fixed travel = floor.distance - move.skin;
        if (travel.raw() < 0) {
          travel = Fixed::zero();
        }
        result.pose.position = result.pose.position + down * travel;
        result.on_floor = true;
      }
    }
  }

  return result;
}

}  // namespace Blunder
