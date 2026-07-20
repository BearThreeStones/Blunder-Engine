#pragma once

#include "EASTL/string.h"

#include "runtime/core/math/math_types.h"
#include "runtime/function/scene/entity_id.h"

namespace Blunder {

/// Runtime entity with local TRS and parent link (EntityId, not raw pointers).
class Entity final {
 public:
  Entity() = default;

  const eastl::string& getName() const { return m_name; }
  void setName(eastl::string name) { m_name = eastl::move(name); }

  const Vec3& getPosition() const { return m_position; }
  void setPosition(const Vec3& position) { m_position = position; }

  const Quat& getRotation() const { return m_rotation; }
  void setRotation(const Quat& rotation) { m_rotation = rotation; }

  const Vec3& getScale() const { return m_scale; }
  void setScale(const Vec3& scale) { m_scale = scale; }

  EntityId getParentId() const { return m_parent_id; }
  void setParentId(EntityId parent_id) { m_parent_id = parent_id; }

  bool isEnabled() const { return m_enabled; }
  void setEnabled(bool enabled) { m_enabled = enabled; }

  bool isTombstoned() const { return m_tombstoned; }
  void setTombstoned(bool tombstoned) { m_tombstoned = tombstoned; }

  /// Mesh Asset Reference (GUID preferred; may be a legacy path).
  const eastl::string& getMeshVirtualPath() const { return m_mesh_virtual_path; }
  void setMeshVirtualPath(eastl::string path) {
    m_mesh_virtual_path = eastl::move(path);
  }

  Mat4 getLocalMatrix() const;

 private:
  eastl::string m_name;
  Vec3 m_position{0.0f};
  Quat m_rotation{glm::identity<Quat>()};
  Vec3 m_scale{1.0f, 1.0f, 1.0f};
  EntityId m_parent_id{k_invalid_entity_id};
  eastl::string m_mesh_virtual_path;
  bool m_enabled{true};
  bool m_tombstoned{false};
};

}  // namespace Blunder
