#pragma once

#include "function/physics/physics_types.h"

namespace Blunder {

class PhysicsWorld final {
 public:
  [[nodiscard]] static PhysicsWorld* create();
  void destroy();

  void step(Fixed dt = Fixed::from_int(1) / Fixed::from_int(60));

  void setGravity(FixedVec3 gravity);
  [[nodiscard]] FixedVec3 getGravity() const;

  [[nodiscard]] RigidBodyHandle createRigidBody(MotionType motion_type, PhysicsTransform pose, Fixed mass);
  void destroyRigidBody(RigidBodyHandle body);

  [[nodiscard]] PhysicsTransform getPose(RigidBodyHandle body) const;
  void setPose(RigidBodyHandle body, PhysicsTransform pose);

  [[nodiscard]] FixedVec3 getLinearVelocity(RigidBodyHandle body) const;
  [[nodiscard]] Fixed getMass(RigidBodyHandle body) const;

  void applyForce(RigidBodyHandle body, FixedVec3 force);
  void applyImpulse(RigidBodyHandle body, FixedVec3 impulse);
  void clearForces(RigidBodyHandle body);

  void setKinematicTarget(RigidBodyHandle body, PhysicsTransform target);

  [[nodiscard]] ColliderHandle attachBoxCollider(RigidBodyHandle body, FixedVec3 half_extents,
                                                 PhysicsMaterial material = {});
  [[nodiscard]] ColliderHandle attachSphereCollider(RigidBodyHandle body, Fixed radius,
                                                    PhysicsMaterial material = {});
  [[nodiscard]] ColliderHandle attachCapsuleCollider(RigidBodyHandle body, Fixed radius, Fixed half_height,
                                                     PhysicsMaterial material = {});
  void destroyCollider(ColliderHandle collider);

  [[nodiscard]] PhysicsMaterial getColliderMaterial(ColliderHandle collider) const;
  [[nodiscard]] ColliderShape getColliderShape(ColliderHandle collider) const;
  [[nodiscard]] bool isColliderValid(ColliderHandle collider) const;

 private:
  PhysicsWorld() = default;

  struct Impl;
  Impl* m_impl = nullptr;
};

}  // namespace Blunder
