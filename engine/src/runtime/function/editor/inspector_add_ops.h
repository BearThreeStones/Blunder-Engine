#pragma once

#include "EASTL/string.h"
#include "EASTL/vector.h"

#include "runtime/core/object/animation_player.h"
#include "runtime/function/scene/camera_component.h"
#include "runtime/function/scene/entity_id.h"

namespace Blunder {

class AssetManager;
class Object;
class SceneInstance;

enum class InspectorUniqueKind {
  Camera,
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
  bool already_present{false};
};

struct InspectorUniqueRemoveSnapshot {
  CameraComponent camera{};
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

bool applyInspectorRemoveClip(
    SceneInstance& scene, EntityId entity_id, size_t index,
    eastl::vector<AnimationPlayer::ClipBinding>& out_before,
    eastl::vector<AnimationPlayer::ClipBinding>& out_after);

}  // namespace Blunder
