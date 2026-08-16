#include "runtime/function/editor/inspector_add_ops.h"

#include <cstddef>

#include "runtime/core/object/animation_tree.h"
#include "runtime/core/object/object.h"
#include "runtime/core/object/skeleton.h"
#include "runtime/function/editor/inspector_animation_player_ops.h"
#include "runtime/function/scene/scene_instance.h"
#include "runtime/function/scene/skeleton_from_gltf.h"

namespace Blunder {
namespace {

bool sceneHasMainCamera(const SceneInstance& scene) {
  bool found = false;
  scene.forEachCamera([&](EntityId, const CameraComponent& camera) {
    if (camera.is_main) {
      found = true;
    }
  });
  return found;
}

void clearOtherMainCameras(SceneInstance& scene, EntityId keep_id) {
  scene.forEachCamera([&](EntityId entity_id, const CameraComponent& camera) {
    if (entity_id == keep_id || !camera.is_main) {
      return;
    }
    CameraComponent updated = camera;
    updated.is_main = false;
    scene.setCamera(entity_id, eastl::move(updated));
  });
}

void ensureSkeletonHydrated(AssetManager* asset_manager, SceneInstance& scene,
                            EntityId entity_id, Object* object,
                            InspectorUniqueAddResult& result) {
  if (object == nullptr || object->hasSkeleton()) {
    return;
  }
  Skeleton* skeleton = object->ensureSkeleton();
  result.created_skeleton = true;
  if (skeleton != nullptr) {
    hydrateSkeletonFromEntityMesh(asset_manager, scene, entity_id, *skeleton);
  }
}

}  // namespace

bool parseInspectorUniqueKind(const eastl::string& name,
                              InspectorUniqueKind& out_kind) {
  if (name == "Camera") {
    out_kind = InspectorUniqueKind::Camera;
    return true;
  }
  if (name == "Skeleton") {
    out_kind = InspectorUniqueKind::Skeleton;
    return true;
  }
  if (name == "AnimationPlayer") {
    out_kind = InspectorUniqueKind::AnimationPlayer;
    return true;
  }
  if (name == "AnimationTree") {
    out_kind = InspectorUniqueKind::AnimationTree;
    return true;
  }
  return false;
}

bool isSkeletonRemoveBlocked(const Object* object) {
  if (object == nullptr) {
    return false;
  }
  return object->hasAnimationPlayer() || object->hasAnimationTree() ||
         object->getSkeletonModifierCount() > 0;
}

InspectorUniqueAddResult applyInspectorUniqueAdd(
    AssetManager* asset_manager, SceneInstance& scene, EntityId entity_id,
    InspectorUniqueKind kind) {
  InspectorUniqueAddResult result{};
  if (!isValid(entity_id) || scene.getEntity(entity_id) == nullptr) {
    return result;
  }

  if (kind == InspectorUniqueKind::Camera) {
    if (scene.getCamera(entity_id) != nullptr) {
      result.already_present = true;
      return result;
    }
    CameraComponent camera;
    camera.is_main = !sceneHasMainCamera(scene);
    if (camera.is_main) {
      clearOtherMainCameras(scene, entity_id);
    }
    scene.setCamera(entity_id, camera);
    result.created_camera = true;
    return result;
  }

  const bool had_object = scene.findBoundObject(entity_id) != nullptr;
  Object* object = scene.ensureBoundObject(entity_id);
  if (object == nullptr) {
    return result;
  }
  result.created_object = !had_object;

  if (kind == InspectorUniqueKind::Skeleton) {
    if (object->hasSkeleton()) {
      result.already_present = true;
      result.created_object = false;
      return result;
    }
    ensureSkeletonHydrated(asset_manager, scene, entity_id, object, result);
    return result;
  }

  if (kind == InspectorUniqueKind::AnimationPlayer) {
    if (object->hasAnimationPlayer()) {
      result.already_present = true;
      result.created_object = false;
      return result;
    }
    ensureSkeletonHydrated(asset_manager, scene, entity_id, object, result);
    object->ensureAnimationPlayer();
    result.created_player = true;
    return result;
  }

  if (kind == InspectorUniqueKind::AnimationTree) {
    if (object->hasAnimationTree()) {
      result.already_present = true;
      result.created_object = false;
      return result;
    }
    if (!object->hasAnimationPlayer()) {
      ensureSkeletonHydrated(asset_manager, scene, entity_id, object, result);
      object->ensureAnimationPlayer();
      result.created_player = true;
    }
    AnimationTree* tree = object->ensureAnimationTree();
    if (tree != nullptr) {
      tree->setActive(false);
      tree->setAssetGuid(eastl::string{});
    }
    result.created_tree = true;
  }

  return result;
}

void undoInspectorUniqueAdd(SceneInstance& scene, EntityId entity_id,
                            const InspectorUniqueAddResult& created) {
  if (created.created_camera) {
    scene.clearCamera(entity_id);
  }

  Object* object = scene.findBoundObject(entity_id);
  if (object != nullptr) {
    if (created.created_tree) {
      object->clearAnimationTree();
    }
    if (created.created_player) {
      object->clearAnimationPlayer();
    }
    if (created.created_skeleton) {
      object->clearSkeleton();
    }
  }

  if (created.created_object) {
    scene.releaseBoundObject(entity_id);
  }
}

bool applyInspectorUniqueRemove(AssetManager* /*asset_manager*/, SceneInstance& scene,
                                EntityId entity_id, InspectorUniqueKind kind,
                                InspectorUniqueRemoveSnapshot& out_snapshot) {
  out_snapshot = {};
  if (kind == InspectorUniqueKind::Camera) {
    const CameraComponent* camera = scene.getCamera(entity_id);
    if (camera == nullptr) {
      return false;
    }
    out_snapshot.camera = *camera;
    scene.clearCamera(entity_id);
    return true;
  }

  Object* object = scene.findBoundObject(entity_id);
  if (object == nullptr) {
    return false;
  }

  if (kind == InspectorUniqueKind::Skeleton) {
    if (!object->hasSkeleton() || isSkeletonRemoveBlocked(object)) {
      return false;
    }
    object->clearSkeleton();
    return true;
  }

  if (kind == InspectorUniqueKind::AnimationPlayer) {
    if (!object->hasAnimationPlayer()) {
      return false;
    }
    out_snapshot.player_clips = object->getAnimationPlayer()->getClipBindings();
    object->clearAnimationPlayer();
    return true;
  }

  if (kind == InspectorUniqueKind::AnimationTree) {
    if (!object->hasAnimationTree()) {
      return false;
    }
    out_snapshot.tree_asset_guid = object->getAnimationTree()->getAssetGuid();
    out_snapshot.tree_active = object->getAnimationTree()->isActive();
    object->clearAnimationTree();
    return true;
  }

  return false;
}

void undoInspectorUniqueRemove(AssetManager* asset_manager, SceneInstance& scene,
                               EntityId entity_id, InspectorUniqueKind kind,
                               const InspectorUniqueRemoveSnapshot& snapshot) {
  if (kind == InspectorUniqueKind::Camera) {
    scene.setCamera(entity_id, snapshot.camera);
    return;
  }

  Object* object = scene.ensureBoundObject(entity_id);
  if (object == nullptr) {
    return;
  }

  if (kind == InspectorUniqueKind::Skeleton) {
    Skeleton* skeleton = object->ensureSkeleton();
    if (skeleton != nullptr) {
      hydrateSkeletonFromEntityMesh(asset_manager, scene, entity_id, *skeleton);
    }
    return;
  }

  if (kind == InspectorUniqueKind::AnimationPlayer) {
    applyClipBindingsToObject(object, snapshot.player_clips);
    return;
  }

  if (kind == InspectorUniqueKind::AnimationTree) {
    AnimationTree* tree = object->ensureAnimationTree();
    if (tree != nullptr) {
      tree->setAssetGuid(snapshot.tree_asset_guid);
      tree->setActive(snapshot.tree_active);
    }
  }
}

bool applyInspectorAddClip(
    SceneInstance& scene, EntityId entity_id,
    eastl::vector<AnimationPlayer::ClipBinding>& out_before,
    eastl::vector<AnimationPlayer::ClipBinding>& out_after) {
  Object* object = scene.findBoundObject(entity_id);
  if (object == nullptr || !object->hasAnimationPlayer()) {
    return false;
  }
  out_before = clipBindingsFromObject(object);
  out_after = out_before;
  AnimationPlayer::ClipBinding draft{};
  out_after.push_back(eastl::move(draft));
  applyClipBindingsToObject(object, out_after);
  return true;
}

bool applyInspectorRemoveClip(
    SceneInstance& scene, EntityId entity_id, size_t index,
    eastl::vector<AnimationPlayer::ClipBinding>& out_before,
    eastl::vector<AnimationPlayer::ClipBinding>& out_after) {
  Object* object = scene.findBoundObject(entity_id);
  if (object == nullptr || !object->hasAnimationPlayer()) {
    return false;
  }
  out_before = clipBindingsFromObject(object);
  if (index >= out_before.size()) {
    return false;
  }
  out_after = out_before;
  out_after.erase(out_after.begin() + static_cast<ptrdiff_t>(index));
  applyClipBindingsToObject(object, out_after);
  return true;
}

}  // namespace Blunder
