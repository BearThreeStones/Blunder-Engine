#include "function/physics/physics_world.h"

#include "function/physics/physics_collision.h"

#include <cassert>
#include <vector>

namespace Blunder {
namespace {

Fixed defaultGravityZ() {
  return -(Fixed::from_int(981) / Fixed::from_int(100));
}

Fixed absFixed(Fixed value) { return value.raw() < 0 ? -value : value; }

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
  bool is_sleeping = false;
  uint32_t sleep_counter = 0;
  bool kinematic_moved = false;
  FixedVec3 kinematic_delta{};
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

struct ContactConstraint {
  uint32_t body_a = UINT32_MAX;
  uint32_t body_b = UINT32_MAX;
  FixedVec3 normal{};
  Fixed penetration = Fixed::zero();
  Fixed friction = Fixed::zero();
  Fixed restitution = Fixed::zero();
};

constexpr int kSolverIterations = 10;
constexpr uint32_t kSleepFrames = 30;
constexpr int kSleepVelocityThresholdRaw = Fixed::kOne / 8;  // 0.125 m/s

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

ColliderWorldShape buildWorldShape(const RigidBodyState& body, const ColliderState& collider) {
  ColliderWorldShape shape{};
  shape.shape = collider.shape;
  shape.pose = body.pose;
  shape.box_half_extents = collider.box_half_extents;
  shape.sphere_radius = collider.sphere_radius;
  shape.capsule_radius = collider.capsule_radius;
  shape.capsule_half_height = collider.capsule_half_height;
  return shape;
}

bool shouldSkipPair(const RigidBodyState& a, const RigidBodyState& b) {
  if (a.motion_type == MotionType::Static && b.motion_type == MotionType::Static) {
    return true;
  }
  if (a.motion_type == MotionType::Dynamic && a.is_sleeping && b.motion_type == MotionType::Dynamic && b.is_sleeping) {
    return true;
  }
  return false;
}

Fixed getInvMass(const RigidBodyState& body, bool allow_sleeping = false) {
  if (body.motion_type != MotionType::Dynamic) {
    return Fixed::zero();
  }
  if (body.is_sleeping && !allow_sleeping) {
    return Fixed::zero();
  }
  if (body.mass.raw() == 0) {
    return Fixed::zero();
  }
  return Fixed::from_int(1) / body.mass;
}

void wakeBody(RigidBodyState& body) {
  if (body.motion_type != MotionType::Dynamic) {
    return;
  }
  body.is_sleeping = false;
  body.sleep_counter = 0;
}

void detectContacts(const std::vector<RigidBodyState>& bodies, const std::vector<ColliderState>& colliders,
                    std::vector<ContactConstraint>& out_contacts) {
  out_contacts.clear();

  for (uint32_t i = 0; i < colliders.size(); ++i) {
    if (!colliders[i].alive) {
      continue;
    }
    for (uint32_t j = i + 1; j < colliders.size(); ++j) {
      if (!colliders[j].alive) {
        continue;
      }

      const ColliderState& collider_a = colliders[i];
      const ColliderState& collider_b = colliders[j];
      if (collider_a.body == collider_b.body) {
        continue;
      }

      const RigidBodyState* body_a = findBody(bodies, collider_a.body);
      const RigidBodyState* body_b = findBody(bodies, collider_b.body);
      if (body_a == nullptr || body_b == nullptr) {
        continue;
      }
      if (shouldSkipPair(*body_a, *body_b)) {
        continue;
      }

      const ColliderWorldShape shape_a = buildWorldShape(*body_a, collider_a);
      const ColliderWorldShape shape_b = buildWorldShape(*body_b, collider_b);
      const ContactManifold manifold = collide(shape_a, shape_b);
      if (!manifold.valid) {
        continue;
      }

      ContactConstraint contact{};
      contact.body_a = collider_a.body.index;
      contact.body_b = collider_b.body.index;
      contact.normal = manifold.normal;
      contact.penetration = manifold.penetration;
      contact.friction = collider_a.material.friction;
      if (collider_b.material.friction.raw() > contact.friction.raw()) {
        contact.friction = collider_b.material.friction;
      }
      contact.restitution = collider_a.material.restitution;
      if (collider_b.material.restitution.raw() > contact.restitution.raw()) {
        contact.restitution = collider_b.material.restitution;
      }
      out_contacts.push_back(contact);
    }
  }
}

void wakeDynamicsNearKinematic(std::vector<RigidBodyState>& bodies, const std::vector<ContactConstraint>& contacts) {
  for (const ContactConstraint& contact : contacts) {
    RigidBodyState& body_a = bodies[contact.body_a];
    RigidBodyState& body_b = bodies[contact.body_b];
    if (body_a.motion_type == MotionType::Kinematic && body_a.kinematic_moved &&
        body_b.motion_type == MotionType::Dynamic) {
      wakeBody(body_b);
    }
    if (body_b.motion_type == MotionType::Kinematic && body_b.kinematic_moved &&
        body_a.motion_type == MotionType::Dynamic) {
      wakeBody(body_a);
    }
  }
}

void carryDynamicsOnKinematic(std::vector<RigidBodyState>& bodies,
                              const std::vector<ContactConstraint>& contacts, Fixed dt) {
  for (const ContactConstraint& contact : contacts) {
    RigidBodyState& body_a = bodies[contact.body_a];
    RigidBodyState& body_b = bodies[contact.body_b];

    if (body_a.motion_type == MotionType::Kinematic && body_a.kinematic_moved &&
        body_b.motion_type == MotionType::Dynamic) {
      wakeBody(body_b);
      body_b.pose.position = body_b.pose.position + body_a.kinematic_delta;
      body_b.linear_velocity = body_b.linear_velocity + body_a.kinematic_delta / dt;
    }
    if (body_b.motion_type == MotionType::Kinematic && body_b.kinematic_moved &&
        body_a.motion_type == MotionType::Dynamic) {
      wakeBody(body_a);
      body_a.pose.position = body_a.pose.position + body_b.kinematic_delta;
      body_a.linear_velocity = body_a.linear_velocity + body_b.kinematic_delta / dt;
    }
  }
}

void wakeContactParticipants(std::vector<RigidBodyState>& bodies,
                             const std::vector<ContactConstraint>& contacts) {
  for (const ContactConstraint& contact : contacts) {
    if (bodies[contact.body_a].motion_type == MotionType::Dynamic) {
      wakeBody(bodies[contact.body_a]);
    }
    if (bodies[contact.body_b].motion_type == MotionType::Dynamic) {
      wakeBody(bodies[contact.body_b]);
    }
  }
}

void solveContacts(std::vector<RigidBodyState>& bodies, const std::vector<ContactConstraint>& contacts, Fixed dt) {
  const Fixed slop = Fixed::from_int(1) / Fixed::from_int(100);
  const Fixed percent = Fixed::from_int(2) / Fixed::from_int(10);

  wakeContactParticipants(bodies, contacts);
  wakeDynamicsNearKinematic(bodies, contacts);

  for (int iteration = 0; iteration < kSolverIterations; ++iteration) {
    for (const ContactConstraint& contact : contacts) {
      RigidBodyState& body_a = bodies[contact.body_a];
      RigidBodyState& body_b = bodies[contact.body_b];

      const Fixed inv_mass_a = getInvMass(body_a);
      const Fixed inv_mass_b = getInvMass(body_b);
      const Fixed total_inv_mass = inv_mass_a + inv_mass_b;

      if (contact.penetration.raw() > slop.raw() && total_inv_mass.raw() > 0) {
        const Fixed correction_mag =
            (contact.penetration - slop) * percent / total_inv_mass;
        const FixedVec3 correction = contact.normal * correction_mag;
        if (inv_mass_a.raw() > 0) {
          body_a.pose.position = body_a.pose.position - correction * inv_mass_a;
        }
        if (inv_mass_b.raw() > 0) {
          body_b.pose.position = body_b.pose.position + correction * inv_mass_b;
        }
      }

      FixedVec3 relative_velocity = body_b.linear_velocity - body_a.linear_velocity;
      const Fixed normal_velocity = dot(relative_velocity, contact.normal);
      if (normal_velocity.raw() < 0 && total_inv_mass.raw() > 0) {
        const Fixed impulse_scalar =
            -(Fixed::from_int(1) + contact.restitution) * normal_velocity / total_inv_mass;
        const FixedVec3 impulse = contact.normal * impulse_scalar;
        if (inv_mass_a.raw() > 0) {
          body_a.linear_velocity = body_a.linear_velocity - impulse * inv_mass_a;
        }
        if (inv_mass_b.raw() > 0) {
          body_b.linear_velocity = body_b.linear_velocity + impulse * inv_mass_b;
        }

        relative_velocity = body_b.linear_velocity - body_a.linear_velocity;
        const Fixed tangent_velocity = dot(relative_velocity, contact.normal);
        FixedVec3 tangent = relative_velocity - contact.normal * tangent_velocity;
        const Fixed tangent_len_sq = dot(tangent, tangent);
        if (tangent_len_sq.raw() > 0 && contact.friction.raw() > 0) {
          const Fixed tangent_len = sqrt(tangent_len_sq);
          const FixedVec3 tangent_dir = tangent / tangent_len;
          Fixed friction_impulse = -dot(relative_velocity, tangent_dir) / total_inv_mass;
          const Fixed max_friction = absFixed(impulse_scalar) * contact.friction;
          if (absFixed(friction_impulse).raw() > max_friction.raw()) {
            friction_impulse = friction_impulse.raw() > 0 ? max_friction : -max_friction;
          }
          const FixedVec3 friction_vec = tangent_dir * friction_impulse;
          if (inv_mass_a.raw() > 0) {
            body_a.linear_velocity = body_a.linear_velocity - friction_vec * inv_mass_a;
          }
          if (inv_mass_b.raw() > 0) {
            body_b.linear_velocity = body_b.linear_velocity + friction_vec * inv_mass_b;
          }
        }
      }

    }
  }

  (void)dt;
}

void updateSleep(std::vector<RigidBodyState>& bodies) {
  for (RigidBodyState& body : bodies) {
    if (!body.alive || body.motion_type != MotionType::Dynamic) {
      continue;
    }

    const Fixed speed_sq = dot(body.linear_velocity, body.linear_velocity);
    const Fixed threshold = Fixed::from_raw(kSleepVelocityThresholdRaw);
    if (speed_sq.raw() <= threshold.raw() * threshold.raw()) {
      ++body.sleep_counter;
      if (body.sleep_counter >= kSleepFrames) {
        body.is_sleeping = true;
        body.linear_velocity = FixedVec3{};
      }
    } else {
      body.sleep_counter = 0;
      body.is_sleeping = false;
    }
  }
}

}  // namespace

struct PhysicsWorld::Impl {
  FixedVec3 gravity = FixedVec3(Fixed::zero(), Fixed::zero(), defaultGravityZ());
  std::vector<RigidBodyState> bodies;
  std::vector<ColliderState> colliders;
  std::vector<ContactConstraint> contacts;
  std::vector<ContactConstraint> pre_kinematic_contacts;
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
  wakeBody(*body);
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
  wakeBody(*body);
  body->accumulated_force = body->accumulated_force + force;
}

void PhysicsWorld::applyImpulse(RigidBodyHandle body_handle, FixedVec3 impulse) {
  RigidBodyState* body = findBody(m_impl->bodies, body_handle);
  assert(body != nullptr);
  if (body->motion_type != MotionType::Dynamic) {
    return;
  }
  wakeBody(*body);
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

bool PhysicsWorld::isBodySleeping(RigidBodyHandle body_handle) const {
  const RigidBodyState* body = findBody(m_impl->bodies, body_handle);
  assert(body != nullptr);
  return body->is_sleeping;
}

void PhysicsWorld::step(Fixed dt) {
  for (RigidBodyState& body : m_impl->bodies) {
    if (!body.alive) {
      continue;
    }

    body.kinematic_moved = false;
    body.kinematic_delta = FixedVec3{};

    if (body.motion_type == MotionType::Static) {
      body.linear_velocity = FixedVec3{};
      continue;
    }

    if (body.motion_type != MotionType::Dynamic || body.is_sleeping) {
      continue;
    }

    FixedVec3 total_force = body.accumulated_force;
    if (body.mass.raw() != 0) {
      total_force = total_force + m_impl->gravity * body.mass;
    }

    const Fixed inv_mass = Fixed::from_int(1) / body.mass;
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

  detectContacts(m_impl->bodies, m_impl->colliders, m_impl->pre_kinematic_contacts);

  for (RigidBodyState& body : m_impl->bodies) {
    if (!body.alive || body.motion_type != MotionType::Kinematic) {
      continue;
    }

    if (body.has_kinematic_target) {
      const FixedVec3 delta_position = body.kinematic_target.position - body.pose.position;
      body.linear_velocity = delta_position / dt;
      body.pose = body.kinematic_target;
      body.has_kinematic_target = false;
      body.kinematic_delta = delta_position;
      body.kinematic_moved =
          delta_position.x.raw() != 0 || delta_position.y.raw() != 0 || delta_position.z.raw() != 0;
    }
  }

  carryDynamicsOnKinematic(m_impl->bodies, m_impl->pre_kinematic_contacts, dt);
  detectContacts(m_impl->bodies, m_impl->colliders, m_impl->contacts);
  solveContacts(m_impl->bodies, m_impl->contacts, dt);
  updateSleep(m_impl->bodies);
}

}  // namespace Blunder
