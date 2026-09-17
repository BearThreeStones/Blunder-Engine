#pragma once

#include "EASTL/string.h"
#include "EASTL/unordered_map.h"
#include "EASTL/vector.h"

#include "runtime/core/math/geometry.h"
#include "runtime/core/math/math_types.h"
#include "runtime/core/object/entity_store.h"
#include "runtime/core/object/object_id.h"
#include "runtime/function/scene/entity.h"
#include "runtime/function/scene/camera_component.h"
#include "runtime/function/scene/character_controller_component.h"
#include "runtime/function/scene/collider_component.h"
#include "runtime/function/scene/fog_component.h"
#include "runtime/function/scene/light_component.h"
#include "runtime/function/scene/mesh_renderer_component.h"
#include "runtime/function/scene/play_camera_resolve.h"
#include "runtime/function/scene/entity_id.h"
#include "runtime/function/scene/scene.h"

namespace Blunder {

class MeshLoader;
class Object;
class Skeleton;

/// Runtime container for entities spawned from a Scene asset (Stride SceneInstance).
class SceneInstance final : public IEntityStore {
 public:
  SceneInstance() = default;
  ~SceneInstance();

  /// False when the entity loop aborted (overlay close / heartbeat stop).
  bool instantiate(const Scene& scene);
  /// True only after a full `instantiate` pass. Attach must not run otherwise.
  bool instantiateCompleted() const { return m_instantiate_completed; }
  void clear();

  void setParent(SceneInstance* parent);
  SceneInstance* getParent() const { return m_parent_instance; }

  void setRootTransform(const Vec3& position, const Quat& rotation,
                      const Vec3& scale);
  Mat4 getSceneToWorldMatrix() const;

  const eastl::string& getSourcePath() const { return m_source_path; }
  void setSourcePath(eastl::string path) { m_source_path = eastl::move(path); }

  EntityId createEntity(eastl::string name, const Vec3& position,
                        const Quat& rotation, const Vec3& scale,
                        EntityId parent_id = k_invalid_entity_id) override;

  bool getTransform(EntityId id, Vec3& out_position, Quat& out_rotation,
                    Vec3& out_scale) const override;
  bool setTransform(EntityId id, const Vec3& position, const Quat& rotation,
                    const Vec3& scale) override;

  /// Soft-delete: keep EntityId stable; hide from editable document.
  bool softDeleteEntity(EntityId id);
  bool restoreEntity(EntityId id);
  bool isTombstoned(EntityId id) const;
  /// True if this entity or any ancestor is tombstoned (hidden from document).
  bool isOmittedFromDocument(EntityId id) const;

  bool isObjectActive(EntityId id) const;
  void setObjectActive(EntityId id, bool active);
  /// Object Active and every ancestor is Object Active, and not tombstoned.
  bool isActiveInHierarchy(EntityId id) const;

  const Entity* getEntity(EntityId id) const;
  Entity* getEntity(EntityId id);
  EntityId findEntityByName(const eastl::string& name) const;

  size_t getEntityCount() const { return m_entities.size(); }
  Mat4 getWorldMatrix(EntityId id) const;

  void markTransformsDirty() override { m_world_matrices_dirty = true; }
  bool isWorldMatricesDirty() const { return m_world_matrices_dirty; }
  void markPhysicsDirty() { m_physics_dirty = true; }
  bool consumePhysicsDirty() {
    const bool dirty = m_physics_dirty;
    m_physics_dirty = false;
    return dirty;
  }
  void ensureWorldMatrices() {
    if (m_world_matrices_dirty) {
      rebuildWorldMatrices();
    }
  }

  EntityId getEntityIdAtIndex(size_t index) const;

  template <typename Fn>
  void forEachEntity(const Fn& fn) const {
    for (size_t i = 0; i < m_entities.size(); ++i) {
      fn(indexToId(i), m_entities[i]);
    }
  }

  template <typename Fn>
  void forEachChild(EntityId parent_id, const Fn& fn) const {
    for (size_t i = 0; i < m_entities.size(); ++i) {
      if (m_entities[i].getParentId() == parent_id) {
        fn(indexToId(i), m_entities[i]);
      }
    }
  }

  bool exportToScene(Scene& out_scene) const;

  void setMeshRenderer(EntityId id, MeshRendererComponent renderer);
  const MeshRendererComponent* getMeshRenderer(EntityId id) const;
  /// Bind pending Mesh Loader keys once those unique meshes are CPU-resident.
  void bindStreamedMeshes(MeshLoader& loader);
  /// After dropScene, restamp or re-submit unique keys still pending on this instance.
  void requeuePendingMeshes(MeshLoader& loader);
  /// Copy MeshAsset materials onto MeshRenderers after deferred hydrate.
  /// Skips renderers that already hold a textured override.
  void rebindMeshRendererMaterialsFromMeshes();
  template <typename Fn>
  void forEachMeshRenderer(const Fn& fn) const {
    for (const auto& entry : m_mesh_renderers) {
      if (isOmittedFromDocument(entry.first)) {
        continue;
      }
      fn(entry.first, entry.second);
    }
  }

  void setCamera(EntityId id, CameraComponent camera);
  const CameraComponent* getCamera(EntityId id) const;
  void clearCamera(EntityId id);
  template <typename Fn>
  void forEachCamera(const Fn& fn) const {
    for (const auto& entry : m_cameras) {
      if (isTombstoned(entry.first)) {
        continue;
      }
      fn(entry.first, entry.second);
    }
  }

  void setLight(EntityId id, LightComponent light);
  const LightComponent* getLight(EntityId id) const;
  void clearLight(EntityId id);
  template <typename Fn>
  void forEachLight(const Fn& fn) const {
    for (const auto& entry : m_lights) {
      if (isTombstoned(entry.first)) {
        continue;
      }
      fn(entry.first, entry.second);
    }
  }

  void setFog(EntityId id, FogComponent fog);
  const FogComponent* getFog(EntityId id) const;
  void clearFog(EntityId id);
  template <typename Fn>
  void forEachFog(const Fn& fn) const {
    for (const auto& entry : m_fogs) {
      if (isTombstoned(entry.first)) {
        continue;
      }
      fn(entry.first, entry.second);
    }
  }

  void setCollider(EntityId id, ColliderComponent collider);
  const ColliderComponent* getCollider(EntityId id) const;
  void clearCollider(EntityId id);
  template <typename Fn>
  void forEachCollider(const Fn& fn) const {
    for (const auto& entry : m_colliders) {
      if (isTombstoned(entry.first)) {
        continue;
      }
      fn(entry.first, entry.second);
    }
  }

  void setCharacterController(EntityId id, CharacterControllerComponent cct);
  const CharacterControllerComponent* getCharacterController(EntityId id) const;
  CharacterControllerComponent* getCharacterController(EntityId id);
  void clearCharacterController(EntityId id);
  template <typename Fn>
  void forEachCharacterController(const Fn& fn) const {
    for (const auto& entry : m_character_controllers) {
      if (isTombstoned(entry.first)) {
        continue;
      }
      fn(entry.first, entry.second);
    }
  }

  const eastl::vector<eastl::string>& getGroups(EntityId id) const;
  void setGroups(EntityId id, eastl::vector<eastl::string> groups);
  void addGroup(EntityId id, const eastl::string& name);
  void removeGroup(EntityId id, const eastl::string& name);
  bool isInGroup(EntityId id, const eastl::string& name) const;
  void findBoundObjectsInGroup(const eastl::string& name,
                               eastl::vector<Object*>& out_objects) const;

  bool hasWorldBounds() const { return m_has_world_bounds; }
  const AABB& getWorldBounds() const { return m_world_bounds; }
  void setWorldBounds(const AABB& bounds);
  /// Authored .scene.asset loads do not go through glTF import, so world
  /// bounds stay unset and AABB camera snap never frames centimetre Sponza.
  /// Build the AABB from MeshRenderer local bounds * world matrix.
  bool rebuildWorldBoundsFromMeshes();

  void tick(float delta_time);

  Object* findBoundObject(EntityId entity_id) const;
  Object* ensureBoundObject(EntityId entity_id);
  void releaseBoundObject(EntityId entity_id);
  eastl::string getDefaultAnimationClipName(EntityId entity_id) const;
  /// Walks entity parents for a bound Object with a Skeleton.
  Skeleton* findSkeletonForEntity(EntityId entity_id) const;

 private:
  EntityId indexToId(size_t index) const;
  size_t idToIndex(EntityId id) const;
  bool validateParentChains() const;
  void rebuildWorldMatrices();

  eastl::string m_source_path;
  SceneInstance* m_parent_instance{nullptr};
  Vec3 m_root_position{0.0f};
  Quat m_root_rotation{glm::identity<Quat>()};
  Vec3 m_root_scale{1.0f, 1.0f, 1.0f};

  eastl::vector<Entity> m_entities;
  eastl::vector<Mat4> m_world_matrices;
  eastl::unordered_map<eastl::string, EntityId> m_name_to_id;
  eastl::unordered_map<EntityId, MeshRendererComponent> m_mesh_renderers;
  eastl::unordered_map<EntityId, CameraComponent> m_cameras;
  eastl::unordered_map<EntityId, LightComponent> m_lights;
  eastl::unordered_map<EntityId, FogComponent> m_fogs;
  eastl::unordered_map<EntityId, ColliderComponent> m_colliders;
  eastl::unordered_map<EntityId, CharacterControllerComponent> m_character_controllers;
  /// Objects created for Behaviour-bearing entities; destroyed on clear().
  eastl::vector<ObjectId> m_bound_object_ids;
  eastl::unordered_map<EntityId, ObjectId> m_bound_object_ids_by_entity;
  eastl::unordered_map<EntityId, eastl::string> m_default_animation_clip_names;
  AABB m_world_bounds{};
  bool m_has_world_bounds{false};
  bool m_world_matrices_dirty{true};
  bool m_instantiate_completed{false};
  bool m_physics_dirty{true};
};

/// Prefer Main camera; else first camera in ascending EntityId order (stable).
inline ResolvedPlayCamera resolvePlayCameraFromScene(const SceneInstance& scene,
                                                     float aspect) {
  eastl::vector<PlayCameraResolveInput> cams;
  scene.forEachEntity([&](EntityId id, const Entity&) {
    if (!scene.isActiveInHierarchy(id)) {
      return;
    }
    const CameraComponent* cam = scene.getCamera(id);
    if (cam == nullptr) {
      return;
    }
    PlayCameraResolveInput in;
    in.entity_id = id;
    in.world = scene.getWorldMatrix(id);
    in.camera = *cam;
    cams.push_back(in);
  });
  return resolvePlayCamera(cams.data(), cams.size(), aspect);
}

}  // namespace Blunder
