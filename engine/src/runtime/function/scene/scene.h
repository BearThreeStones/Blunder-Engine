#pragma once

#include "EASTL/string.h"
#include "EASTL/vector.h"

#include "runtime/core/math/math_types.h"
#include "runtime/core/object/behaviour_id.h"
#include "runtime/core/reflection/variant.h"

namespace Blunder {

/// One JSON property bag entry (bool / number / string only).
struct SceneBehaviourProperty final {
  eastl::string key;
  Variant value;
};

/// Ordered Behaviour declaration persisted on a scene entity.
struct SceneBehaviourDeclaration final {
  eastl::string type;
  BehaviourId id{k_invalid_behaviour_id};
  eastl::vector<SceneBehaviourProperty> properties;
};

/// Static entity definition deserialized from a Scene asset.
struct SceneEntityDefinition final {
  eastl::string name;
  Vec3 position{0.0f};
  Quat rotation{glm::identity<Quat>()};
  Vec3 scale{1.0f, 1.0f, 1.0f};
  eastl::string parent_name;
  /// Mesh Asset Reference: preferred GUID; may briefly hold a legacy
  /// `assets/...mesh.yaml` path until migration on load/save.
  eastl::string mesh_virtual_path;
  /// Ordered Behaviour list; empty when the JSON key is absent (legacy).
  eastl::vector<SceneBehaviourDeclaration> behaviours;
};

/// Reference to a nested child scene (loaded explicitly by SceneSystem).
struct SceneChildReference final {
  eastl::string scene_virtual_path;
  eastl::string instance_name;
  Vec3 position{0.0f};
  Quat rotation{glm::identity<Quat>()};
  Vec3 scale{1.0f, 1.0f, 1.0f};
};

/// Static scene data: entity templates and child scene references.
class Scene final {
 public:
  Scene() = default;

  const eastl::vector<SceneEntityDefinition>& getEntities() const {
    return m_entities;
  }
  eastl::vector<SceneEntityDefinition>& getEntities() { return m_entities; }

  const eastl::vector<SceneChildReference>& getChildScenes() const {
    return m_child_scenes;
  }
  eastl::vector<SceneChildReference>& getChildScenes() { return m_child_scenes; }

  const eastl::string& getName() const { return m_name; }
  void setName(eastl::string name) { m_name = eastl::move(name); }

  /// Scene Asset GUID (persisted as top-level `"guid"` in `.scene.asset` JSON).
  const eastl::string& getGuid() const { return m_guid; }
  void setGuid(eastl::string guid) { m_guid = eastl::move(guid); }

 private:
  eastl::string m_name;
  eastl::string m_guid;
  eastl::vector<SceneEntityDefinition> m_entities;
  eastl::vector<SceneChildReference> m_child_scenes;
};

}  // namespace Blunder
