#pragma once

#include "EASTL/string.h"
#include "EASTL/vector.h"

#include "runtime/core/math/math_types.h"
#include "runtime/core/object/behaviour_id.h"
#include "runtime/core/reflection/variant.h"
#include "runtime/function/scene/camera_component.h"

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
  /// One AnimationPlayer clip name→GUID binding (serialized under
  /// `animationPlayer.clips`).
  struct AnimationClipBinding final {
    eastl::string name;
    eastl::string guid;
  };

  /// AnimationClip Asset GUIDs referenced by this entity (derived from
  /// `animation_player_clips` on save; graph uses these for consumer→clip edges).
  eastl::vector<eastl::string> animation_clip_guids;
  /// When true, runtime instantiates a Skeleton on the entity Object.
  bool has_skeleton{false};
  /// AnimationPlayer name→GUID map; empty when the player key is absent.
  eastl::vector<AnimationClipBinding> animation_player_clips;
  /// Phase 2 defaults persisted under `animationPlayer` (not live playback).
  float animation_player_time_scale{1.0f};
  eastl::string animation_player_slot0;
  eastl::string animation_player_slot1;
  float animation_player_blend_weight{0.0f};
  /// BlendSpace1D point on a named node (serialized under `animationTree`).
  struct AnimationTreeBlendSpacePointDef final {
    eastl::string clip_name;
    float scalar{0.0f};
  };
  /// BlendSpace1D node topology + authored scalar drive.
  struct AnimationTreeBlendSpaceDef final {
    eastl::string node_name;
    eastl::vector<AnimationTreeBlendSpacePointDef> points;
    float scalar{0.0f};
  };
  /// StateMachine state: single clip or BlendSpace1D node playback.
  struct AnimationTreeStateDef final {
    eastl::string name;
    /// `"clip"` or `"blendSpace1D"`.
    eastl::string kind;
    eastl::string clip_name;
    eastl::string blend_space_node;
  };
  /// Scene-embedded AnimationTree topology (no standalone Tree Asset).
  bool has_animation_tree{false};
  /// Optional AnimationTree Asset GUID; when set, Asset is topology base.
  eastl::string animation_tree_asset_guid;
  bool animation_tree_active{false};
  eastl::string animation_tree_current_state;
  eastl::string animation_tree_base_blend_space_node;
  eastl::string animation_tree_add2_clip;
  float animation_tree_add2_weight{0.0f};
  /// Authored OneShot clip slot (not live playback state).
  eastl::string animation_tree_oneshot_clip;
  eastl::vector<AnimationTreeBlendSpaceDef> animation_tree_blend_spaces;
  eastl::vector<AnimationTreeStateDef> animation_tree_states;
  /// Ordered Behaviour list; empty when the JSON key is absent (legacy).
  eastl::vector<SceneBehaviourDeclaration> behaviours;
  bool has_camera{false};
  CameraComponent camera{};
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
