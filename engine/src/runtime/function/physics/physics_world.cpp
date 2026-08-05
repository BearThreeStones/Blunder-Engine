#include "function/physics/physics_world.h"

#include <cassert>
#include <vector>

namespace Blunder {
namespace {

Fixed defaultGravityZ() {
  return -(Fixed::from_int(981) / Fixed::from_int(100));
}

struct RigidBodyState {
  MotionType motion_type = MotionType::Dynamic;
  PhysicsTransform pose{};
  PhysicsTransform kinematic_target{};
  bool has_kinematic_target = false;
  FixedVec3 linear_velocity{};
  Fixed mass = Fixed::from_int(1);
  FixedVec3 accumulated_force{};
  FixedVec3 accumulated_impulse{};
  bool alive = false;
  uint32_t generation = 0;
};

struct ColliderState {
  RigidBodyHandle body{};
  ColliderShape shape = ColliderShape::Box;
  PhysicsMaterial material{};
  FixedVec3 box_half_extents{};
  Fixed sphere_radius = Fixed::zero();
  Fixed capsule_radius = Fixed::zero();
  Fixed capsule_half_height = Fixed::zero();
  bool alive = false;
  uint32_t generation = 0;
};

RigidBodyState* findBody(std::vector<RigidBodyState>& bodies, RigidBodyHandle handle) {
  if (!handle.isValid() || handle.index >= bodies.size()) {
    return nullptr;
  }
  RigidBodyState& body = bodies[handle.index];
  if (!body.alive || body.generation != handle.generation) {
    return nullptr;
  }
  return &body;
}

const RigidBodyState* findBody(const std::vector<RigidBodyState>& bodies, RigidBodyHandle handle) {
  if (!handle.isValid() || handle.index >= bodies.size()) {
    return nullptr;
  }
  const RigidBodyState& body = bodies[handle.index];
  if (!body.alive || body.generation != handle.generation) {
    return nullptr;
  }
  return &body;
}

ColliderState* findCollider(std::vector<ColliderState>& colliders, ColliderHandle handle) {
  if (!handle.isValid() || handle.index >= colliders.size()) {
    return nullptr;
  }
  ColliderState& collider = colliders[handle.index];
  if (!collider.alive || collider.generation != handle.generation) {
    return nullptr;
  }
  return &collider;
}

const ColliderState* findCollider(const std::vector<ColliderState>& colliders, ColliderHandle handle) {
  if (!handle.isValid() || handle.index >= colliders.size()) {
    return nullptr;
  }
  const ColliderState& collider = colliders[handle.index];
  if (!collider.alive || collider.generation != handle.generation) {
    return nullptr;
  }
  return &collider;
}

}  // namespace

struct PhysicsWorld::Impl {
  FixedVec3 gravity = FixedVec3(Fixed::zero(), Fixed::zero(), defaultGravityZ());
  std::vector<RigidBodyState> bodies;
  std::vector<ColliderState> colliders;
};

PhysicsWorld* PhysicsWorld::create() {
  auto* world = new PhysicsWorld();
  world->m_impl = new Impl();
  return world;
}

void PhysicsWorld::destroy() {
  delete m_impl;
  m_impl = nullptr;
  delete this;
}

void PhysicsWorld::setGravity(FixedVec3 gravity) { m_impl->gravity = gravity; }

FixedVec3 PhysicsWorld::getGravity() const { return m_impl->gravity; }

RigidBodyHandle PhysicsWorld::createRigidBody(MotionType motion_type, PhysicsTransform pose, Fixed mass) {
  RigidBodyState body{};
  body.motion_type = motion_type;
  body.pose = pose;
  body.mass = motion_type == MotionType::Static ? Fixed::zero() : mass;
  body.alive = true;

  if (m_impl->bodies.size() < m_impl->bodies.capacity()) {
    for (uint32_t i = 0; i < m_impl->bodies.size(); ++i) {
      if (!m_impl->bodies[i].alive) {
        body.generation = m_impl->bodies[i].generation + 1;
        m_impl->bodies[i] = body;
        return RigidBodyHandle{i, body.generation};
      }
    }
  }

  const uint32_t index = static_cast<uint32_t>(m_impl->bodies.size());
  m_impl->bodies.push_back(body);
  return RigidBodyHandle{index, body.generation};
}

void PhysicsWorld::destroyRigidBody(RigidBodyHandle body_handle) {
  RigidBodyState* body = findBody(m_impl->bodies, body_handle);
  if (body == nullptr) {
    return;
  }

  for (ColliderState& collider : m_impl->colliders) {
    if (collider.alive && collider.body == body_handle) {
      collider.alive = false;
      ++collider.generation;
    }
  }

  body->alive = false;
  ++body->generation;
}

PhysicsTransform PhysicsWorld::getPose(RigidBodyHandle body_handle) const {
  const RigidBodyState* body = findBody(m_impl->bodies, body_handle);
  assert(body != nullptr);
  return body->pose;
}

void PhysicsWorld::setPose(RigidBodyHandle body_handle, PhysicsTransform pose) {
  RigidBodyState* body = findBody(m_impl->bodies, body_handle);
  assert(body != nullptr);
  body->pose = pose;
}

FixedVec3 PhysicsWorld::getLinearVelocity(RigidBodyHandle body_handle) const {
  const RigidBodyState* body = findBody(m_impl->bodies, body_handle);
  assert(body != nullptr);
  return body->linear_velocity;
}

Fixed PhysicsWorld::getMass(RigidBodyHandle body_handle) const {
  const RigidBodyState* body = findBody(m_impl->bodies, body_handle);
  assert(body != nullptr);
  return body->mass;
}

void PhysicsWorld::applyForce(RigidBodyHandle body_handle, FixedVec3 force) {
  RigidBodyState* body = findBody(m_impl->bodies, body_handle);
  assert(body != nullptr);
  if (body->motion_type != MotionType::Dynamic) {
    return;
  }
  body->accumulated_force = body->accumulated_force + force;
}

void PhysicsWorld::applyImpulse(RigidBodyHandle body_handle, FixedVec3 impulse) {
  RigidBodyState* body = findBody(m_impl->bodies, body_handle);
  assert(body != nullptr);
  if (body->motion_type != MotionType::Dynamic) {
    return;
  }
  body->accumulated_impulse = body->accumulated_impulse + impulse;
}

void PhysicsWorld::clearForces(RigidBodyHandle body_handle) {
  RigidBodyState* body = findBody(m_impl->bodies, body_handle);
  assert(body != nullptr);
  body->accumulated_force = FixedVec3{};
  body->accumulated_impulse = FixedVec3{};
}

void PhysicsWorld::setKinematicTarget(RigidBodyHandle body_handle, PhysicsTransform target) {
  RigidBodyState* body = findBody(m_impl->bodies, body_handle);
  assert(body != nullptr);
  assert(body->motion_type == MotionType::Kinematic);
  body->kinematic_target = target;
  body->has_kinematic_target = true;
}

ColliderHandle PhysicsWorld::attachBoxCollider(RigidBodyHandle body_handle, FixedVec3 half_extents,
                                               PhysicsMaterial material) {
  assert(findBody(m_impl->bodies, body_handle) != nullptr);

  ColliderState collider{};
  collider.body = body_handle;
  collider.shape = ColliderShape::Box;
  collider.material = material;
  collider.box_half_extents = half_extents;
  collider.alive = true;

  if (m_impl->colliders.size() < m_impl->colliders.capacity()) {
    for (uint32_t i = 0; i < m_impl->colliders.size(); ++i) {
      if (!m_impl->colliders[i].alive) {
        collider.generation = m_impl->colliders[i].generation + 1;
        m_impl->colliders[i] = collider;
        return ColliderHandle{i, collider.generation};
      }
    }
  }

  const uint32_t index = static_cast<uint32_t>(m_impl->colliders.size());
  m_impl->colliders.push_back(collider);
  return ColliderHandle{index, collider.generation};
}

ColliderHandle PhysicsWorld::attachSphereCollider(RigidBodyHandle body_handle, Fixed radius,
                                                    PhysicsMaterial material) {
  assert(findBody(m_impl->bodies, body_handle) != nullptr);

  ColliderState collider{};
  collider.body = body_handle;
  collider.shape = ColliderShape::Sphere;
  collider.material = material;
  collider.sphere_radius = radius;
  collider.alive = true;

  if (m_impl->colliders.size() < m_impl->colliders.capacity()) {
    for (uint32_t i = 0; i < m_impl->colliders.size(); ++i) {
      if (!m_impl->colliders[i].alive) {
        collider.generation = m_impl->colliders[i].generation + 1;
        m_impl->colliders[i] = collider;
        return ColliderHandle{i, collider.generation};
      }
    }
  }

  const uint32_t index = static_cast<uint32_t>(m_impl->colliders.size());
  m_impl->colliders.push_back(collider);
  return ColliderHandle{index, collider.generation};
}

ColliderHandle PhysicsWorld::attachCapsuleCollider(RigidBodyHandle body_handle, Fixed radius, Fixed half_height,
                                                     PhysicsMaterial material) {
  assert(findBody(m_impl->bodies, body_handle) != nullptr);

  ColliderState collider{};
  collider.body = body_handle;
  collider.shape = ColliderShape::Capsule;
  collider.material = material;
  collider.capsule_radius = radius;
  collider.capsule_half_height = half_height;
  collider.alive = true;

  if (m_impl->colliders.size() < m_impl->colliders.capacity()) {
    for (uint32_t i = 0; i < m_impl->colliders.size(); ++i) {
      if (!m_impl->colliders[i].alive) {
        collider.generation = m_impl->colliders[i].generation + 1;
        m_impl->colliders[i] = collider;
        return ColliderHandle{i, collider.generation};
      }
    }
  }

  const uint32_t index = static_cast<uint32_t>(m_impl->colliders.size());
  m_impl->colliders.push_back(collider);
  return ColliderHandle{index, collider.generation};
}

void PhysicsWorld::destroyCollider(ColliderHandle collider_handle) {
  ColliderState* collider = findCollider(m_impl->colliders, collider_handle);
  if (collider == nullptr) {
    return;
  }
  collider->alive = false;
  ++collider->generation;
}

PhysicsMaterial PhysicsWorld::getColliderMaterial(ColliderHandle collider_handle) const {
  const ColliderState* collider = findCollider(m_impl->colliders, collider_handle);
  assert(collider != nullptr);
  return collider->material;
}

ColliderShape PhysicsWorld::getColliderShape(ColliderHandle collider_handle) const {
  const ColliderState* collider = findCollider(m_impl->colliders, collider_handle);
  assert(collider != nullptr);
  return collider->shape;
}

bool PhysicsWorld::isColliderValid(ColliderHandle collider_handle) const {
  return findCollider(m_impl->colliders, collider_handle) != nullptr;
}

void PhysicsWorld::step(Fixed dt) {
  for (RigidBodyState& body : m_impl->bodies) {
    if (!body.alive) {
      continue;
    }

    if (body.motion_type == MotionType::Kinematic && body.has_kinematic_target) {
      const FixedVec3 delta_position = body.kinematic_target.position - body.pose.position;
      body.linear_velocity = delta_position / dt;
      body.pose = body.kinematic_target;
      body.has_kinematic_target = false;
      continue;
    }

    if (body.motion_type == MotionType::Static) {
      body.linear_velocity = FixedVec3{};
      continue;
    }

    if (body.motion_type != MotionType::Dynamic) {
      continue;
    }

    FixedVec3 total_force = body.accumulated_force;
    if (body.mass.raw() != 0) {
      total_force = total_force + m_impl->gravity * body.mass;
    }

    const Fixed inv_mass = body.mass.raw() != 0 ? Fixed::from_int(1) / body.mass : Fixed::zero();
    const FixedVec3 acceleration = total_force * inv_mass;
    body.linear_velocity = body.linear_velocity + acceleration * dt;

    if (body.accumulated_impulse.x.raw() != 0 || body.accumulated_impulse.y.raw() != 0 ||
        body.accumulated_impulse.z.raw() != 0) {
      body.linear_velocity = body.linear_velocity + body.accumulated_impulse * inv_mass;
    }

    body.pose.position = body.pose.position + body.linear_velocity * dt;

    body.accumulated_force = FixedVec3{};
    body.accumulated_impulse = FixedVec3{};
  }
}

}  // namespace Blunder
