#include "runtime/function/physics/physics_manager.h"

#include "runtime/function/scene/character_controller_component.h"
#include "runtime/function/scene/collider_component.h"
#include "runtime/function/scene/entity.h"
#include "runtime/function/scene/scene_instance.h"
#include "runtime/core/object/object.h"
#include "runtime/core/object/object_db.h"

#include <glm/gtc/matrix_inverse.hpp>
#include <glm/gtc/matrix_transform.hpp>

namespace Blunder {
namespace {

constexpr float kPhysicsDt = 1.0f / 60.0f;

MotionType motionTypeFromBody(ColliderBodyKind kind) {
  switch (kind) {
    case ColliderBodyKind::Kinematic:
      return MotionType::Kinematic;
    case ColliderBodyKind::Area:
    case ColliderBodyKind::Static:
    default:
      return MotionType::Static;
  }
}

Quat rotationFromWorld(const Mat4& world) {
  const Mat3 rot(world);
  return glm::normalize(glm::quat_cast(rot));
}

Vec3 translationFromWorld(const Mat4& world) {
  return Vec3(world[3]);
}

}  // namespace

FixedVec3 physicsVecFromFloat(const Vec3& value) {
  return FixedVec3(fixedFromFloat(value.x), fixedFromFloat(value.y), fixedFromFloat(value.z));
}

Vec3 floatVecFromPhysics(FixedVec3 value) {
  return Vec3(floatFromFixed(value.x), floatFromFixed(value.y), floatFromFixed(value.z));
}

PhysicsTransform physicsPoseFromWorld(const Mat4& world) {
  PhysicsTransform pose{};
  const Vec3 position = translationFromWorld(world);
  const Quat rotation = rotationFromWorld(world);
  pose.position = physicsVecFromFloat(position);
  pose.rotation = FixedQuat(fixedFromFloat(rotation.x), fixedFromFloat(rotation.y),
                            fixedFromFloat(rotation.z), fixedFromFloat(rotation.w));
  return pose;
}

PhysicsManager::~PhysicsManager() { clear(); }

void PhysicsManager::destroyBinding(WorldBinding& binding) {
  if (binding.world != nullptr) {
    binding.world->destroy();
    binding.world = nullptr;
  }
  binding.bodies.clear();
  binding.colliders.clear();
  binding.accumulator = 0.0f;
}

void PhysicsManager::clear() {
  for (auto& entry : m_worlds) {
    destroyBinding(entry.second);
  }
  m_worlds.clear();
}

void PhysicsManager::unbind(const SceneInstance* scene) {
  if (scene == nullptr) {
    return;
  }
  const auto it = m_worlds.find(scene);
  if (it == m_worlds.end()) {
    return;
  }
  destroyBinding(it->second);
  m_worlds.erase(it);
}

void PhysicsManager::fillHit(const SceneInstance& scene, const PhysicsQueryHit& kernel,
                             PhysicsSceneHit& out_hit) const {
  out_hit = {};
  if (!kernel.hit) {
    return;
  }
  out_hit.hit = true;
  out_hit.distance = floatFromFixed(kernel.distance);
  out_hit.point = floatVecFromPhysics(kernel.point);
  out_hit.normal = floatVecFromPhysics(kernel.normal);
  out_hit.is_area = kernel.is_area;
  out_hit.entity_id = static_cast<EntityId>(kernel.user_data);
  if (isValid(out_hit.entity_id)) {
    out_hit.groups = scene.getGroups(out_hit.entity_id);
    if (const Object* object = scene.findBoundObject(out_hit.entity_id)) {
      out_hit.object_id = static_cast<uint64_t>(object->getId());
    }
  }
}

void PhysicsManager::attachCollider(WorldBinding& binding, SceneInstance& scene,
                                    EntityId entity_id) {
  const ColliderComponent* collider = scene.getCollider(entity_id);
  if (collider == nullptr || scene.isOmittedFromDocument(entity_id) ||
      !scene.isActiveInHierarchy(entity_id)) {
    return;
  }
  if (collider->shape == ColliderShapeKind::TriangleMesh &&
      collider->body_kind != ColliderBodyKind::Static) {
    return;
  }
  if (collider->shape == ColliderShapeKind::TriangleMesh && collider->triangles.empty()) {
    return;
  }

  const PhysicsTransform pose = physicsPoseFromWorld(scene.getWorldMatrix(entity_id));
  const MotionType motion = motionTypeFromBody(collider->body_kind);
  const RigidBodyHandle body = binding.world->createRigidBody(motion, pose, Fixed::from_int(1));
  ColliderHandle handle{};
  if (collider->shape == ColliderShapeKind::Box) {
    handle = binding.world->attachBoxCollider(body, physicsVecFromFloat(collider->box_half_extents));
  } else if (collider->shape == ColliderShapeKind::Sphere) {
    handle = binding.world->attachSphereCollider(body, fixedFromFloat(collider->sphere_radius));
  } else if (collider->shape == ColliderShapeKind::Capsule) {
    handle = binding.world->attachCapsuleCollider(body, fixedFromFloat(collider->capsule_radius),
                                                  fixedFromFloat(colliderCapsuleHalfHeight(*collider)));
  } else {
    eastl::vector<PhysicsTriangle> tris;
    tris.reserve(collider->triangles.size());
    for (const ColliderTriangle& src : collider->triangles) {
      PhysicsTriangle tri{};
      tri.v0 = physicsVecFromFloat(src.v0);
      tri.v1 = physicsVecFromFloat(src.v1);
      tri.v2 = physicsVecFromFloat(src.v2);
      tris.push_back(tri);
    }
    handle = binding.world->attachTriangleMeshCollider(body, tris.data(),
                                                       static_cast<uint32_t>(tris.size()));
  }
  if (!handle.isValid()) {
    binding.world->destroyRigidBody(body);
    return;
  }
  binding.world->setColliderLayer(handle, collider->layer);
  binding.world->setColliderMask(handle, collider->mask);
  binding.world->setColliderQueryOnly(handle, collider->body_kind == ColliderBodyKind::Area);
  binding.world->setColliderUserData(handle, static_cast<uint64_t>(entity_id));
  binding.bodies[entity_id] = body;
  binding.colliders[entity_id] = handle;
}

void PhysicsManager::rebuild(SceneInstance& scene) {
  scene.ensureWorldMatrices();
  WorldBinding& binding = m_worlds[&scene];
  destroyBinding(binding);
  binding.world = PhysicsWorld::create();
  scene.forEachCollider([&](EntityId entity_id, const ColliderComponent&) {
    attachCollider(binding, scene, entity_id);
  });
}

PhysicsWorld* PhysicsManager::worldFor(const SceneInstance* scene) const {
  if (scene == nullptr) {
    return nullptr;
  }
  const auto it = m_worlds.find(scene);
  if (it == m_worlds.end()) {
    return nullptr;
  }
  return it->second.world;
}

PhysicsWorld* PhysicsManager::ensureWorld(SceneInstance& scene) {
  auto it = m_worlds.find(&scene);
  if (it == m_worlds.end() || it->second.world == nullptr || scene.consumePhysicsDirty()) {
    rebuild(scene);
  }
  return worldFor(&scene);
}

void PhysicsManager::syncEntityPose(SceneInstance& scene, EntityId entity_id) {
  auto it = m_worlds.find(&scene);
  if (it == m_worlds.end() || it->second.world == nullptr) {
    return;
  }
  const auto body_it = it->second.bodies.find(entity_id);
  if (body_it == it->second.bodies.end()) {
    return;
  }
  it->second.world->setPose(body_it->second, physicsPoseFromWorld(scene.getWorldMatrix(entity_id)));
}

void PhysicsManager::tick(float dt, bool play_host, bool paused) {
  for (auto& entry : m_worlds) {
    WorldBinding& binding = entry.second;
    if (binding.world == nullptr) {
      continue;
    }
    SceneInstance* scene = const_cast<SceneInstance*>(entry.first);
    if (scene == nullptr) {
      continue;
    }
    scene->forEachCollider([&](EntityId entity_id, const ColliderComponent&) {
      syncEntityPose(*scene, entity_id);
    });
    if (!play_host || paused) {
      continue;
    }
    binding.accumulator += dt;
    const Fixed step_dt = Fixed::from_int(1) / Fixed::from_int(60);
    while (binding.accumulator >= kPhysicsDt) {
      binding.world->step(step_dt);
      binding.accumulator -= kPhysicsDt;
    }
  }
}

bool PhysicsManager::raycast(SceneInstance& scene, const Vec3& origin, const Vec3& direction,
                             float max_distance, uint32_t mask, bool collide_with_areas,
                             PhysicsSceneHit& out_hit) {
  PhysicsWorld* world = ensureWorld(scene);
  out_hit = {};
  if (world == nullptr) {
    return false;
  }
  PhysicsQueryHit kernel{};
  const bool ok =
      world->raycast(physicsVecFromFloat(origin), physicsVecFromFloat(direction),
                     fixedFromFloat(max_distance), mask, collide_with_areas, kernel);
  fillHit(scene, kernel, out_hit);
  return ok;
}

bool PhysicsManager::shapecast(SceneInstance& scene, PhysicsSweepShape sweep_shape,
                               const Vec3& origin, const Quat& rotation,
                               const Vec3& box_half_extents, float sphere_radius,
                               float capsule_radius, float capsule_half_height,
                               const Vec3& direction, float max_distance, uint32_t mask,
                               bool collide_with_areas, PhysicsSceneHit& out_hit) {
  PhysicsWorld* world = ensureWorld(scene);
  out_hit = {};
  if (world == nullptr) {
    return false;
  }
  PhysicsTransform pose{};
  pose.position = physicsVecFromFloat(origin);
  pose.rotation = FixedQuat(fixedFromFloat(rotation.x), fixedFromFloat(rotation.y),
                            fixedFromFloat(rotation.z), fixedFromFloat(rotation.w));
  PhysicsQueryHit kernel{};
  const bool ok = world->shapecast(
      sweep_shape, pose, physicsVecFromFloat(box_half_extents), fixedFromFloat(sphere_radius),
      fixedFromFloat(capsule_radius), fixedFromFloat(capsule_half_height),
      physicsVecFromFloat(direction), fixedFromFloat(max_distance), mask, collide_with_areas,
      kernel);
  fillHit(scene, kernel, out_hit);
  return ok;
}

bool PhysicsManager::moveAndSlide(SceneInstance& scene, EntityId entity_id,
                                  const Vec3& displacement) {
  CharacterControllerComponent* controller = scene.getCharacterController(entity_id);
  if (controller == nullptr) {
    return false;
  }
  PhysicsWorld* world = ensureWorld(scene);
  if (world == nullptr) {
    return false;
  }
  PhysicsCharacterMove move{};
  move.pose = physicsPoseFromWorld(scene.getWorldMatrix(entity_id));
  move.displacement = physicsVecFromFloat(displacement);
  move.radius = fixedFromFloat(controller->radius);
  move.half_height = fixedFromFloat(characterControllerHalfHeight(*controller));
  move.snap_length = fixedFromFloat(controller->snap_length);
  move.skin = fixedFromFloat(controller->skin);
  move.mask = controller->mask;
  const PhysicsCharacterResult result = Blunder::moveAndSlide(*world, move);
  controller->on_floor = result.on_floor;
  controller->on_wall = result.on_wall;
  controller->on_ceiling = result.on_ceiling;
  Entity* entity = scene.getEntity(entity_id);
  if (entity == nullptr) {
    return false;
  }
  const Vec3 world_pos = floatVecFromPhysics(result.pose.position);
  // CCT Unique pose follows entity local +Z; write world translation back as local
  // when the entity is unparented (DogWalk fixture). Parented: convert via parent world.
  Vec3 local = world_pos;
  const EntityId parent_id = entity->getParentId();
  if (isValid(parent_id)) {
    const Mat4 parent_world = scene.getWorldMatrix(parent_id);
    const Mat4 inv = glm::inverse(parent_world);
    local = Vec3(inv * Vec4(world_pos, 1.0f));
  }
  scene.setTransform(entity_id, local, entity->getRotation(), entity->getScale());
  syncEntityPose(scene, entity_id);
  return true;
}

}  // namespace Blunder
