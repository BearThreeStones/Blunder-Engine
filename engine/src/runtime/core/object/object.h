#pragma once

#include "EASTL/string.h"
#include "EASTL/vector.h"

#include "runtime/core/math/math_types.h"
#include "runtime/core/object/object_id.h"
#include "runtime/core/reflection/export_macros.h"
#include "runtime/function/scene/entity_id.h"

namespace Blunder {

BLUNDER_CLASS()
class Object {
 public:
  Object() = default;
  virtual ~Object() = default;

  ObjectId getId() const { return m_id; }

  const eastl::string& getName() const { return m_name; }
  void setName(eastl::string name) { m_name = eastl::move(name); }

  bool isEnabled() const { return m_enabled; }
  void setEnabled(bool enabled) { m_enabled = enabled; }

  ObjectId getParentId() const { return m_parent_id; }
  void setParent(Object* parent);

  size_t getChildCount() const { return m_children.size(); }
  ObjectId getChildId(size_t index) const;

  EntityId getEntityId() const { return m_entity_id; }
  void setEntityId(EntityId entity_id) { m_entity_id = entity_id; }
  bool hasEntity() const { return isValid(m_entity_id); }

  void* getScriptPeer() const { return m_script_peer; }
  void setScriptPeer(void* peer) { m_script_peer = peer; }
  void clearScriptPeer() { m_script_peer = nullptr; }

  BLUNDER_PROPERTY()
  Vec3 getPosition() const;
  void setPosition(const Vec3& position);

  BLUNDER_PROPERTY()
  Quat getRotation() const;
  void setRotation(const Quat& rotation);

  BLUNDER_PROPERTY()
  Vec3 getScale() const;
  void setScale(const Vec3& scale);

 private:
  friend class ObjectDB;

  void materializeEntityIfNeeded();
  void syncLocalTransformFromStore();

  ObjectId m_id{k_invalid_object_id};
  eastl::string m_name;
  bool m_enabled{true};
  ObjectId m_parent_id{k_invalid_object_id};
  eastl::vector<ObjectId> m_children;
  EntityId m_entity_id{k_invalid_entity_id};
  void* m_script_peer{nullptr};

  // Local façade until full ECS World lands (lazy TRS before Entity exists).
  Vec3 m_local_position{0.0f};
  Quat m_local_rotation{glm::identity<Quat>()};
  Vec3 m_local_scale{1.0f, 1.0f, 1.0f};
  bool m_has_local_trs{false};
};

}  // namespace Blunder
