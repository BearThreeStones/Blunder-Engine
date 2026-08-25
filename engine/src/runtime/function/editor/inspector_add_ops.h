#pragma once

#include "EASTL/string.h"
#include "EASTL/vector.h"

#include "runtime/core/object/animation_player.h"
#include "runtime/function/scene/camera_component.h"
#include "runtime/function/scene/light_component.h"
#include "runtime/function/scene/entity_id.h"

namespace Blunder {

class AssetManager;
class Object;
class SceneInstance;

enum class InspectorUniqueKind {
  Camera,
  Light,
  Skeleton,
  AnimationPlayer,
  AnimationTree,
};

struct InspectorUniqueAddResult {
  bool created_object{false};
  bool created_skeleton{false};
  bool created_player{false};
  bool created_tree{false};
  bool created_camera{false};
  bool created_light{false};
  bool already_present{false};
};

struct InspectorUniqueRemoveSnapshot {
  CameraComponent camera{};
  LightComponent light{};
  eastl::vector<AnimationPlayer::ClipBinding> player_clips;
  eastl::string tree_asset_guid;
  bool tree_active{false};
};

bool parseInspectorUniqueKind(const eastl::string& name,
                              InspectorUniqueKind& out_kind);

bool isSkeletonRemoveBlocked(const Object* object);

InspectorUniqueAddResult applyInspectorUniqueAdd(
    AssetManager* asset_manager, SceneInstance& scene, EntityId entity_id,
    InspectorUniqueKind kind);

void undoInspectorUniqueAdd(SceneInstance& scene, EntityId entity_id,
                            const InspectorUniqueAddResult& created);

bool applyInspectorUniqueRemove(AssetManager* asset_manager, SceneInstance& scene,
                                EntityId entity_id, InspectorUniqueKind kind,
                                InspectorUniqueRemoveSnapshot& out_snapshot);

void undoInspectorUniqueRemove(AssetManager* asset_manager, SceneInstance& scene,
                               EntityId entity_id, InspectorUniqueKind kind,
                               const InspectorUniqueRemoveSnapshot& snapshot);

bool applyInspectorAddClip(
    SceneInstance& scene, EntityId entity_id,
    eastl::vector<AnimationPlayer::ClipBinding>& out_before,
    eastl::vector<AnimationPlayer::ClipBinding>& out_after);

/// Append one complete Clip Binding (logical name + GUID). Rejects dual-empty
/// and duplicate logical names.
bool applyInspectorAddClipBinding(
    SceneInstance& scene, EntityId entity_id, const eastl::string& clip_name,
    const eastl::string& clip_guid,
    eastl::vector<AnimationPlayer::ClipBinding>& out_before,
    eastl::vector<AnimationPlayer::ClipBinding>& out_after);

/// Retarget row Asset Reference; keeps logical name. Rejects dual-empty GUID.
bool applyInspectorRetargetClipBinding(
    SceneInstance& scene, EntityId entity_id, size_t index,
    const eastl::string& clip_guid,
    eastl::vector<AnimationPlayer::ClipBinding>& out_before,
    eastl::vector<AnimationPlayer::ClipBinding>& out_after);

/// Content Browser drop onto Player clip list: `drop_target` >=0 retarget,
/// -1 append (stem default), -2 miss (no-op).
bool applyInspectorAnimationClipDrop(
    SceneInstance& scene, EntityId entity_id, int drop_target,
    const eastl::string& clip_stem, const eastl::string& clip_guid,
    eastl::vector<AnimationPlayer::ClipBinding>& out_before,
    eastl::vector<AnimationPlayer::ClipBinding>& out_after);

bool applyInspectorRemoveClip(
    SceneInstance& scene, EntityId entity_id, size_t index,
    eastl::vector<AnimationPlayer::ClipBinding>& out_before,
    eastl::vector<AnimationPlayer::ClipBinding>& out_after);

}  // namespace Blunder
