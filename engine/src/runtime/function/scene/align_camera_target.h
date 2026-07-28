#pragma once

#include "EASTL/span.h"

#include "runtime/function/scene/entity_id.h"
#include "runtime/function/scene/entity_id.h"
#include "runtime/function/scene/play_camera_resolve.h"
#include "runtime/function/scene/scene_instance.h"

namespace Blunder {

struct AlignCameraTargetResult {
  bool ok{false};
  EntityId entity_id{k_invalid_entity_id};
};

/// Resolve the Camera entity targeted by Align View / Align Camera.
/// Exactly one selected Camera → that entity; multi-select → fail; no selection
/// → Main Camera if present, else first by ascending EntityId (Play order).
inline AlignCameraTargetResult resolveAlignCameraTarget(
    const SceneInstance& scene, eastl::span<const EntityId> selected_ids) {
  AlignCameraTargetResult result{};
  if (selected_ids.size() > 1) {
    return result;
  }

  if (selected_ids.size() == 1) {
    const EntityId id = selected_ids[0];
    if (scene.getCamera(id) != nullptr && !scene.isTombstoned(id)) {
      result.ok = true;
      result.entity_id = id;
    }
    return result;
  }

  eastl::vector<PlayCameraResolveInput> cameras;
  scene.forEachEntity([&](EntityId id, const Entity&) {
    const CameraComponent* camera = scene.getCamera(id);
    if (camera == nullptr || scene.isTombstoned(id)) {
      return;
    }
    PlayCameraResolveInput input{};
    input.entity_id = id;
    input.camera = *camera;
    cameras.push_back(input);
  });

  const ResolvedPlayCamera resolved =
      resolvePlayCamera(cameras.data(), cameras.size(), 1.0f);
  if (resolved.ok) {
    result.ok = true;
    result.entity_id = resolved.entity_id;
  }
  return result;
}

}  // namespace Blunder
