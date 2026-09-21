#pragma once

#include "EASTL/string.h"
#include "EASTL/unordered_map.h"
#include "EASTL/vector.h"

#include "function/physics/physics_character.h"
#include "function/physics/physics_world.h"
#include "runtime/core/math/math_types.h"
#include "runtime/function/scene/entity_id.h"

namespace Blunder {

class SceneInstance;
class Object;

struct PhysicsSceneHit {
  bool hit{false};
  float distance{0.0f};
  Vec3 point{0.0f};
  Vec3 normal{0.0f, 0.0f, 1.0f};
  EntityId entity_id{k_invalid_entity_id};
  uint64_t object_id{0};
  bool is_area{false};
  eastl::vector<eastl::string> groups;
};

class PhysicsManager final {
 public:
  PhysicsManager() = default;
  ~PhysicsManager();

  void clear();
  void unbind(const SceneInstance* scene);
  void rebuild(SceneInstance& scene);
  void syncEntityPose(SceneInstance& scene, EntityId entity_id);
  void tick(float dt, bool play_host, bool paused);

  [[nodiscard]] PhysicsWorld* worldFor(const SceneInstance* scene) const;
  [[nodiscard]] PhysicsWorld* ensureWorld(SceneInstance& scene);

  bool raycast(SceneInstance& scene, const Vec3& origin, const Vec3& direction, float max_distance,
               uint32_t mask, bool collide_with_areas, PhysicsSceneHit& out_hit);
  bool shapecast(SceneInstance& scene, PhysicsSweepShape sweep_shape, const Vec3& origin,
                 const Quat& rotation, const Vec3& box_half_extents, float sphere_radius,
                 float capsule_radius, float capsule_half_height, const Vec3& direction,
                 float max_distance, uint32_t mask, bool collide_with_areas,
                 PhysicsSceneHit& out_hit);

  bool moveAndSlide(SceneInstance& scene, EntityId entity_id, const Vec3& displacement);

 private:
  struct WorldBinding {
    PhysicsWorld* world{nullptr};
    eastl::unordered_map<EntityId, RigidBodyHandle> bodies;
    eastl::unordered_map<EntityId, ColliderHandle> colliders;
    float accumulator{0.0f};
  };

  void destroyBinding(WorldBinding& binding);
  void fillHit(const SceneInstance& scene, const PhysicsQueryHit& kernel,
               PhysicsSceneHit& out_hit) const;
  void attachCollider(WorldBinding& binding, SceneInstance& scene, EntityId entity_id);

  eastl::unordered_map<const SceneInstance*, WorldBinding> m_worlds;
};

PhysicsTransform physicsPoseFromWorld(const Mat4& world);
FixedVec3 physicsVecFromFloat(const Vec3& value);
Vec3 floatVecFromPhysics(FixedVec3 value);

}  // namespace Blunder
