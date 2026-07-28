#pragma once

#include "EASTL/span.h"

#include "runtime/function/scene/entity_id.h"
#include "runtime/function/scene/play_camera_resolve.h"
#include "runtime/function/scene/scene_instance.h"

namespace Blunder {

struct CameraPreviewTargetResult {
  bool ok{false};
  EntityId entity_id{k_invalid_entity_id};
};

/// Primary if it has a Camera; else first selected entity that has a Camera.
/// Empty selection or no Camera in selection → ok=false.
inline CameraPreviewTargetResult resolveCameraPreviewTarget(
    const SceneInstance& scene, EntityId primary_id,
    eastl::span<const EntityId> selected_ids) {
  CameraPreviewTargetResult result{};
  if (selected_ids.empty()) {
    return result;
  }
  auto has_camera = [&](EntityId id) {
    return id != k_invalid_entity_id && !scene.isTombstoned(id) &&
           scene.getCamera(id) != nullptr;
  };
  if (has_camera(primary_id)) {
    result.ok = true;
    result.entity_id = primary_id;
    return result;
  }
  for (EntityId id : selected_ids) {
    if (has_camera(id)) {
      result.ok = true;
      result.entity_id = id;
      return result;
    }
  }
  return result;
}

inline ResolvedPlayCamera buildCameraPreviewMatrices(const SceneInstance& scene,
                                                     EntityId entity_id,
                                                     float aspect) {
  ResolvedPlayCamera empty{};
  const CameraComponent* cam = scene.getCamera(entity_id);
  if (cam == nullptr || scene.isTombstoned(entity_id)) {
    return empty;
  }
  PlayCameraResolveInput input{};
  input.entity_id = entity_id;
  input.world = scene.getWorldMatrix(entity_id);
  input.camera = *cam;
  return resolvePlayCamera(&input, 1, aspect);
}

}  // namespace Blunder
