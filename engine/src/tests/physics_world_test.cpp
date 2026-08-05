#include "function/physics/physics_world.h"

#include <cassert>
#include <cstdint>

namespace {

using Blunder::ColliderHandle;
using Blunder::ColliderShape;
using Blunder::Fixed;
using Blunder::FixedQuat;
using Blunder::FixedVec3;
using Blunder::MotionType;
using Blunder::PhysicsMaterial;
using Blunder::PhysicsTransform;
using Blunder::PhysicsWorld;
using Blunder::RigidBodyHandle;

Fixed testDt() { return Fixed::from_int(1) / Fixed::from_int(60); }

Fixed testGravityZ() { return -(Fixed::from_int(981) / Fixed::from_int(100)); }

void world_create_destroy() {
  PhysicsWorld* world = PhysicsWorld::create();
  assert(world != nullptr);
  world->destroy();
}

void dynamic_body_free_fall_under_gravity() {
  PhysicsWorld* world = PhysicsWorld::create();

  const RigidBodyHandle body =
      world->createRigidBody(MotionType::Dynamic, PhysicsTransform{}, Fixed::from_int(1));
  assert(body.isValid());

  world->step(testDt());

  const PhysicsTransform pose = world->getPose(body);
  assert(pose.position.z.raw() < 0);

  world->destroy();
}

void default_gravity_is_z_down() {
  PhysicsWorld* world = PhysicsWorld::create();

  const FixedVec3 gravity = world->getGravity();
  assert(gravity.x == Fixed::zero());
  assert(gravity.y == Fixed::zero());
  assert(gravity.z == testGravityZ());

  world->destroy();
}

void gravity_override() {
  PhysicsWorld* world = PhysicsWorld::create();
  world->setGravity(FixedVec3(Fixed::zero(), Fixed::zero(), Fixed::from_int(-5)));

  const FixedVec3 gravity = world->getGravity();
  assert(gravity.z == Fixed::from_int(-5));

  world->destroy();
}

void static_body_does_not_move() {
  PhysicsWorld* world = PhysicsWorld::create();

  const RigidBodyHandle body = world->createRigidBody(MotionType::Static, PhysicsTransform{}, Fixed::zero());
  world->step(testDt());

  const PhysicsTransform pose = world->getPose(body);
  assert(pose.position.z == Fixed::zero());

  world->destroy();
}

void attach_box_collider_default_material() {
  PhysicsWorld* world = PhysicsWorld::create();

  const RigidBodyHandle body =
      world->createRigidBody(MotionType::Dynamic, PhysicsTransform{}, Fixed::from_int(1));
  const ColliderHandle collider = world->attachBoxCollider(
      body, FixedVec3(Fixed::from_int(1), Fixed::from_int(1), Fixed::from_int(1)));
  assert(collider.isValid());

  const PhysicsMaterial material = world->getColliderMaterial(collider);
  assert(material.friction == Fixed::zero());
  assert(material.restitution == Fixed::zero());

  world->destroy();
}

void attach_sphere_and_capsule_colliders() {
  PhysicsWorld* world = PhysicsWorld::create();

  const RigidBodyHandle body =
      world->createRigidBody(MotionType::Dynamic, PhysicsTransform{}, Fixed::from_int(1));
  const ColliderHandle sphere = world->attachSphereCollider(body, Fixed::from_int(1));
  const ColliderHandle capsule =
      world->attachCapsuleCollider(body, Fixed::from_int(1), Fixed::from_int(2));
  assert(sphere.isValid());
  assert(capsule.isValid());
  assert(world->getColliderShape(sphere) == ColliderShape::Sphere);
  assert(world->getColliderShape(capsule) == ColliderShape::Capsule);

  world->destroy();
}

void apply_force_and_clear() {
  PhysicsWorld* world = PhysicsWorld::create();

  const RigidBodyHandle body =
      world->createRigidBody(MotionType::Dynamic, PhysicsTransform{}, Fixed::from_int(2));
  world->applyForce(body, FixedVec3(Fixed::from_int(10), Fixed::zero(), Fixed::zero()));
  world->step(testDt());

  const FixedVec3 velocity = world->getLinearVelocity(body);
  assert(velocity.x.raw() > 0);

  world->clearForces(body);
  const PhysicsTransform pose_before = world->getPose(body);
  world->step(testDt());
  const PhysicsTransform pose_after = world->getPose(body);
  const Fixed delta_x = pose_after.position.x - pose_before.position.x;
  const Fixed dt = testDt();
  assert(delta_x == velocity.x * dt);

  world->destroy();
}

void kinematic_target_pose_sets_velocity_and_position() {
  PhysicsWorld* world = PhysicsWorld::create();

  PhysicsTransform start{};
  start.position.z = Fixed::zero();
  const RigidBodyHandle body = world->createRigidBody(MotionType::Kinematic, start, Fixed::zero());

  PhysicsTransform target{};
  target.position.z = Fixed::from_int(1);
  world->setKinematicTarget(body, target);
  world->step(testDt());

  const PhysicsTransform pose = world->getPose(body);
  assert(pose.position.z == target.position.z);

  const FixedVec3 velocity = world->getLinearVelocity(body);
  assert(velocity.z == Fixed::from_int(1) / testDt());

  world->destroy();
}

void destroy_rigid_body_removes_colliders() {
  PhysicsWorld* world = PhysicsWorld::create();

  const RigidBodyHandle body =
      world->createRigidBody(MotionType::Dynamic, PhysicsTransform{}, Fixed::from_int(1));
  const ColliderHandle collider = world->attachBoxCollider(
      body, FixedVec3(Fixed::from_int(1), Fixed::from_int(1), Fixed::from_int(1)));
  world->destroyRigidBody(body);
  assert(!collider.isValid() || !world->isColliderValid(collider));

  world->destroy();
}

}  // namespace

int main() {
  world_create_destroy();
  dynamic_body_free_fall_under_gravity();
  default_gravity_is_z_down();
  gravity_override();
  static_body_does_not_move();
  attach_box_collider_default_material();
  attach_sphere_and_capsule_colliders();
  apply_force_and_clear();
  kinematic_target_pose_sets_velocity_and_position();
  destroy_rigid_body_removes_colliders();
  return 0;
}
