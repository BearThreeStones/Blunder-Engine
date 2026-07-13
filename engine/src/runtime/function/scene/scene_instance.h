#pragma once

#include "EASTL/string.h"
#include "EASTL/unordered_map.h"
#include "EASTL/vector.h"

#include "runtime/core/math/geometry.h"
#include "runtime/core/math/math_types.h"
#include "runtime/function/scene/entity.h"
#include "runtime/function/scene/mesh_renderer_component.h"
#include "runtime/function/scene/entity_id.h"
#include "runtime/function/scene/scene.h"

namespace Blunder {

/// Runtime container for entities spawned from a Scene asset (Stride SceneInstance).
class SceneInstance final {
 public:
  SceneInstance() = default;

  void instantiate(const Scene& scene);
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
                        EntityId parent_id = k_invalid_entity_id);

  /// Soft-delete: keep EntityId stable; hide from editable document.
  bool softDeleteEntity(EntityId id);
  bool restoreEntity(EntityId id);
  bool isTombstoned(EntityId id) const;

  const Entity* getEntity(EntityId id) const;
  Entity* getEntity(EntityId id);
  EntityId findEntityByName(const eastl::string& name) const;

  size_t getEntityCount() const { return m_entities.size(); }
  Mat4 getWorldMatrix(EntityId id) const;

  void markTransformsDirty() { m_world_matrices_dirty = true; }
  bool isWorldMatricesDirty() const { return m_world_matrices_dirty; }

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
  template <typename Fn>
  void forEachMeshRenderer(const Fn& fn) const {
    for (const auto& entry : m_mesh_renderers) {
      if (isTombstoned(entry.first)) {
        continue;
      }
      fn(entry.first, entry.second);
    }
  }

  bool hasWorldBounds() const { return m_has_world_bounds; }
  const AABB& getWorldBounds() const { return m_world_bounds; }
  void setWorldBounds(const AABB& bounds);

  void tick(float delta_time);

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
  AABB m_world_bounds{};
  bool m_has_world_bounds{false};
  bool m_world_matrices_dirty{true};
};

}  // namespace Blunder
