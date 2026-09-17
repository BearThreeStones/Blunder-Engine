#include "function/physics/physics_character.h"
#include "function/physics/physics_world.h"

#include <cassert>

namespace {

using Blunder::ColliderHandle;
using Blunder::Fixed;
using Blunder::FixedVec3;
using Blunder::MotionType;
using Blunder::PhysicsQueryHit;
using Blunder::PhysicsSweepShape;
using Blunder::PhysicsTriangle;
using Blunder::PhysicsWorld;
using Blunder::RigidBodyHandle;

PhysicsTriangle unitFloor() {
  PhysicsTriangle tri{};
  tri.v0 = FixedVec3(Fixed::from_int(-10), Fixed::from_int(-10), Fixed::zero());
  tri.v1 = FixedVec3(Fixed::from_int(10), Fixed::from_int(-10), Fixed::zero());
  tri.v2 = FixedVec3(Fixed::zero(), Fixed::from_int(10), Fixed::zero());
  return tri;
}

void ray_hits_static_triangle() {
  PhysicsWorld* world = PhysicsWorld::create();
  const PhysicsTriangle floor = unitFloor();
  const RigidBodyHandle body =
      world->createRigidBody(MotionType::Static, {}, Fixed::zero());
  world->attachTriangleMeshCollider(body, &floor, 1);

  PhysicsQueryHit hit{};
  const bool ok =
      world->raycast(FixedVec3(Fixed::zero(), Fixed::zero(), Fixed::from_int(5)),
                     FixedVec3(Fixed::zero(), Fixed::zero(), Fixed::from_int(-1)),
                     Fixed::from_int(20), Blunder::kDefaultColliderMask, false, hit);
  assert(ok);
  assert(hit.hit);
  assert(hit.distance.raw() > 0);
  world->destroy();
}

void sphere_cast_hits_static_box() {
  PhysicsWorld* world = PhysicsWorld::create();
  const RigidBodyHandle body =
      world->createRigidBody(MotionType::Static, {}, Fixed::zero());
  world->attachBoxCollider(body, FixedVec3(Fixed::from_int(1), Fixed::from_int(1), Fixed::from_int(1)));

  Blunder::PhysicsTransform pose{};
  pose.position.z = Fixed::from_int(5);
  PhysicsQueryHit hit{};
  const bool ok = world->shapecast(
      PhysicsSweepShape::Sphere, pose, FixedVec3{}, Fixed::from_int(1) / Fixed::from_int(2),
      Fixed::zero(), Fixed::zero(), FixedVec3(Fixed::zero(), Fixed::zero(), Fixed::from_int(-1)),
      Fixed::from_int(20), Blunder::kDefaultColliderMask, false, hit);
  assert(ok);
  assert(hit.hit);
  world->destroy();
}

void box_shapecast_hits_static_capsule() {
  PhysicsWorld* world = PhysicsWorld::create();
  Blunder::PhysicsTransform capsule_pose{};
  const RigidBodyHandle body =
      world->createRigidBody(MotionType::Static, capsule_pose, Fixed::zero());
  world->attachCapsuleCollider(body, Fixed::from_int(1), Fixed::from_int(1));

  Blunder::PhysicsTransform pose{};
  pose.position.x = Fixed::from_int(6);
  PhysicsQueryHit hit{};
  const bool ok = world->shapecast(
      PhysicsSweepShape::Box, pose,
      FixedVec3(Fixed::from_int(1), Fixed::from_int(1), Fixed::from_int(1)), Fixed::zero(),
      Fixed::zero(), Fixed::zero(), FixedVec3(Fixed::from_int(-1), Fixed::zero(), Fixed::zero()),
      Fixed::from_int(20), Blunder::kDefaultColliderMask, false, hit);
  assert(ok);
  world->destroy();
}

void trimesh_query_shape_rejected() {
  PhysicsWorld* world = PhysicsWorld::create();
  PhysicsQueryHit hit{};
  const bool ok = world->shapecast(
      PhysicsSweepShape::TriangleMesh, {}, FixedVec3{}, Fixed::zero(), Fixed::zero(),
      Fixed::zero(), FixedVec3(Fixed::zero(), Fixed::zero(), Fixed::from_int(-1)),
      Fixed::from_int(1), Blunder::kDefaultColliderMask, false, hit);
  assert(!ok);
  world->destroy();
}

void capsule_cast_hits_static_box() {
  PhysicsWorld* world = PhysicsWorld::create();
  const RigidBodyHandle body =
      world->createRigidBody(MotionType::Static, {}, Fixed::zero());
  world->attachBoxCollider(body, FixedVec3(Fixed::from_int(1), Fixed::from_int(1),
                                           Fixed::from_int(1)));

  Blunder::PhysicsTransform pose{};
  pose.position.z = Fixed::from_int(6);
  PhysicsQueryHit hit{};
  const bool ok = world->shapecast(
      PhysicsSweepShape::Capsule, pose, FixedVec3{}, Fixed::zero(),
      Fixed::from_int(1) / Fixed::from_int(2), Fixed::from_int(1),
      FixedVec3(Fixed::zero(), Fixed::zero(), Fixed::from_int(-1)),
      Fixed::from_int(20), Blunder::kDefaultColliderMask, false, hit);
  assert(ok);
  assert(hit.hit);
  world->destroy();
}

void mask_miss_and_hit() {
  PhysicsWorld* world = PhysicsWorld::create();
  const RigidBodyHandle body =
      world->createRigidBody(MotionType::Static, {}, Fixed::zero());
  const ColliderHandle col =
      world->attachBoxCollider(body, FixedVec3(Fixed::from_int(1), Fixed::from_int(1), Fixed::from_int(1)));
  world->setColliderLayer(col, 1u << 4);

  PhysicsQueryHit miss{};
  const bool missed =
      world->raycast(FixedVec3(Fixed::zero(), Fixed::zero(), Fixed::from_int(5)),
                     FixedVec3(Fixed::zero(), Fixed::zero(), Fixed::from_int(-1)),
                     Fixed::from_int(20), 1u, false, miss);
  assert(!missed);

  PhysicsQueryHit hit{};
  const bool ok =
      world->raycast(FixedVec3(Fixed::zero(), Fixed::zero(), Fixed::from_int(5)),
                     FixedVec3(Fixed::zero(), Fixed::zero(), Fixed::from_int(-1)),
                     Fixed::from_int(20), 1u << 4, false, hit);
  assert(ok);
  world->destroy();
}

void area_default_miss_opt_in_hit() {
  PhysicsWorld* world = PhysicsWorld::create();
  const RigidBodyHandle body =
      world->createRigidBody(MotionType::Static, {}, Fixed::zero());
  const ColliderHandle col =
      world->attachBoxCollider(body, FixedVec3(Fixed::from_int(1), Fixed::from_int(1), Fixed::from_int(1)));
  world->setColliderQueryOnly(col, true);

  PhysicsQueryHit miss{};
  assert(!world->raycast(FixedVec3(Fixed::zero(), Fixed::zero(), Fixed::from_int(5)),
                         FixedVec3(Fixed::zero(), Fixed::zero(), Fixed::from_int(-1)),
                         Fixed::from_int(20), Blunder::kDefaultColliderMask, false, miss));

  PhysicsQueryHit hit{};
  assert(world->raycast(FixedVec3(Fixed::zero(), Fixed::zero(), Fixed::from_int(5)),
                        FixedVec3(Fixed::zero(), Fixed::zero(), Fixed::from_int(-1)),
                        Fixed::from_int(20), Blunder::kDefaultColliderMask, true, hit));
  assert(hit.is_area);
  world->destroy();
}

void area_does_not_block_dynamic() {
  PhysicsWorld* world = PhysicsWorld::create();
  const RigidBodyHandle area_body =
      world->createRigidBody(MotionType::Static, {}, Fixed::zero());
  const ColliderHandle area =
      world->attachBoxCollider(area_body, FixedVec3(Fixed::from_int(2), Fixed::from_int(2), Fixed::from_int(2)));
  world->setColliderQueryOnly(area, true);

  Blunder::PhysicsTransform pose{};
  pose.position.z = Fixed::from_int(1);
  const RigidBodyHandle dynamic =
      world->createRigidBody(MotionType::Dynamic, pose, Fixed::from_int(1));
  world->attachSphereCollider(dynamic, Fixed::from_int(1));
  const Fixed z0 = world->getPose(dynamic).position.z;
  world->step(Fixed::from_int(1) / Fixed::from_int(60));
  assert(world->getPose(dynamic).position.z.raw() < z0.raw());
  world->destroy();
}

void move_and_slide_wall() {
  PhysicsWorld* world = PhysicsWorld::create();
  Blunder::PhysicsTransform wall_pose{};
  wall_pose.position.x = Fixed::from_int(2);
  const RigidBodyHandle wall =
      world->createRigidBody(MotionType::Static, wall_pose, Fixed::zero());
  world->attachBoxCollider(wall, FixedVec3(Fixed::from_int(1), Fixed::from_int(4), Fixed::from_int(4)));

  Blunder::PhysicsCharacterMove move{};
  move.pose.position.z = Fixed::from_int(2);
  move.displacement = FixedVec3(Fixed::from_int(10), Fixed::zero(), Fixed::zero());
  move.radius = Fixed::from_int(1) / Fixed::from_int(2);
  move.half_height = Fixed::from_int(1);
  move.snap_length = Fixed::zero();
  const Blunder::PhysicsCharacterResult result = Blunder::moveAndSlide(*world, move);
  assert(result.pose.position.x.raw() < Fixed::from_int(2).raw());
  assert(result.on_wall);
  world->destroy();
}

void move_and_slide_floor() {
  PhysicsWorld* world = PhysicsWorld::create();
  const PhysicsTriangle floor = unitFloor();
  const RigidBodyHandle body =
      world->createRigidBody(MotionType::Static, {}, Fixed::zero());
  world->attachTriangleMeshCollider(body, &floor, 1);

  Blunder::PhysicsCharacterMove move{};
  move.pose.position.z = Fixed::from_int(2);
  move.displacement = FixedVec3(Fixed::from_int(1), Fixed::zero(), Fixed::zero());
  move.radius = Fixed::from_int(1) / Fixed::from_int(2);
  move.half_height = Fixed::from_int(1);
  move.snap_length = Fixed::from_int(2);
  const Blunder::PhysicsCharacterResult result = Blunder::moveAndSlide(*world, move);
  assert(result.on_floor);
  world->destroy();
}

}  // namespace

int main() {
  ray_hits_static_triangle();
  sphere_cast_hits_static_box();
  box_shapecast_hits_static_capsule();
  capsule_cast_hits_static_box();
  trimesh_query_shape_rejected();
  mask_miss_and_hit();
  area_default_miss_opt_in_hit();
  area_does_not_block_dynamic();
  move_and_slide_wall();
  move_and_slide_floor();
  return 0;
}
