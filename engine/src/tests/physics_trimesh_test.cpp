#include "function/physics/physics_world.h"

#include <cassert>

namespace {

using Blunder::ColliderHandle;
using Blunder::Fixed;
using Blunder::FixedVec3;
using Blunder::MotionType;
using Blunder::PhysicsTriangle;
using Blunder::PhysicsWorld;
using Blunder::RigidBodyHandle;

Fixed testDt() { return Fixed::from_int(1) / Fixed::from_int(60); }

PhysicsTriangle unitFloor() {
  PhysicsTriangle tri{};
  tri.v0 = FixedVec3(Fixed::from_int(-10), Fixed::from_int(-10), Fixed::zero());
  tri.v1 = FixedVec3(Fixed::from_int(10), Fixed::from_int(-10), Fixed::zero());
  tri.v2 = FixedVec3(Fixed::zero(), Fixed::from_int(10), Fixed::zero());
  return tri;
}

void capsule_rests_on_static_trimesh() {
  PhysicsWorld* world = PhysicsWorld::create();
  const PhysicsTriangle floor = unitFloor();
  const RigidBodyHandle mesh_body =
      world->createRigidBody(MotionType::Static, {}, Fixed::zero());
  const ColliderHandle mesh =
      world->attachTriangleMeshCollider(mesh_body, &floor, 1);
  assert(mesh.isValid());

  Blunder::PhysicsTransform capsule_pose{};
  capsule_pose.position.z = Fixed::from_int(4);
  const RigidBodyHandle capsule =
      world->createRigidBody(MotionType::Dynamic, capsule_pose, Fixed::from_int(1));
  world->attachCapsuleCollider(capsule, Fixed::from_int(1), Fixed::from_int(1));

  for (int i = 0; i < 180; ++i) {
    world->step(testDt());
  }

  const Fixed z = world->getPose(capsule).position.z;
  assert(z.raw() > Fixed::from_int(1).raw());
  assert(z.raw() < Fixed::from_int(4).raw());
  world->destroy();
}

void dynamic_trimesh_attach_rejected() {
  PhysicsWorld* world = PhysicsWorld::create();
  const PhysicsTriangle floor = unitFloor();
  const RigidBodyHandle dynamic =
      world->createRigidBody(MotionType::Dynamic, {}, Fixed::from_int(1));
  const ColliderHandle mesh = world->attachTriangleMeshCollider(dynamic, &floor, 1);
  assert(!mesh.isValid());
  world->attachSphereCollider(dynamic, Fixed::from_int(1));
  world->step(testDt());
  assert(world->getPose(dynamic).position.z.raw() < 0);
  world->destroy();
}

void empty_triangle_list_skips() {
  PhysicsWorld* world = PhysicsWorld::create();
  const RigidBodyHandle body =
      world->createRigidBody(MotionType::Static, {}, Fixed::zero());
  const ColliderHandle mesh = world->attachTriangleMeshCollider(body, nullptr, 0);
  assert(!mesh.isValid());
  world->destroy();
}

}  // namespace

int main() {
  capsule_rests_on_static_trimesh();
  dynamic_trimesh_attach_rejected();
  empty_triangle_list_skips();
  return 0;
}
