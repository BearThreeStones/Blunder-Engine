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
  /// Static triangle mesh only. Empty list or non-Static body returns an invalid handle
  /// (no collider, no AABB fallback).
  [[nodiscard]] ColliderHandle attachTriangleMeshCollider(RigidBodyHandle body,
                                                          const PhysicsTriangle* triangles,
                                                          uint32_t triangle_count,
                                                          PhysicsMaterial material = {});
  void destroyCollider(ColliderHandle collider);

  void setColliderLayer(ColliderHandle collider, uint32_t layer);
  void setColliderMask(ColliderHandle collider, uint32_t mask);
  void setColliderQueryOnly(ColliderHandle collider, bool query_only);
  void setColliderUserData(ColliderHandle collider, uint64_t user_data);
  [[nodiscard]] uint32_t getColliderLayer(ColliderHandle collider) const;
  [[nodiscard]] uint32_t getColliderMask(ColliderHandle collider) const;
  [[nodiscard]] bool isColliderQueryOnly(ColliderHandle collider) const;
  [[nodiscard]] uint64_t getColliderUserData(ColliderHandle collider) const;

  [[nodiscard]] bool raycast(FixedVec3 origin, FixedVec3 direction, Fixed max_distance, uint32_t mask,
                             bool collide_with_areas, PhysicsQueryHit& out_hit) const;
  /// Primitive sweep only. `PhysicsSweepShape::TriangleMesh` is rejected (no hit).
  [[nodiscard]] bool shapecast(PhysicsSweepShape sweep_shape, PhysicsTransform pose,
                               FixedVec3 box_half_extents, Fixed sphere_radius, Fixed capsule_radius,
                               Fixed capsule_half_height, FixedVec3 direction, Fixed max_distance,
                               uint32_t mask, bool collide_with_areas, PhysicsQueryHit& out_hit) const;

  [[nodiscard]] PhysicsMaterial getColliderMaterial(ColliderHandle collider) const;
  [[nodiscard]] ColliderShape getColliderShape(ColliderHandle collider) const;
  [[nodiscard]] bool isColliderValid(ColliderHandle collider) const;
  [[nodiscard]] bool isBodySleeping(RigidBodyHandle body) const;

 private:
  PhysicsWorld() = default;

  struct Impl;
  Impl* m_impl = nullptr;
};

}  // namespace Blunder
