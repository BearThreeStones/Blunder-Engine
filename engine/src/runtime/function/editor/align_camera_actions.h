#pragma once

#include "EASTL/span.h"
#include "EASTL/vector.h"

#include "runtime/function/scene/entity_id.h"

namespace Blunder {

class EditorCamera;
class SceneInstance;

/// Match editor viewport to the resolved target camera (no document history).
bool alignViewToCamera(EditorCamera& editor_camera, const SceneInstance& scene,
                       eastl::span<const EntityId> selected_ids);

/// Write target camera pose + vertical FOV from the editor viewport (with history).
bool alignCameraToView(SceneInstance* scene, EditorCamera& editor_camera,
                       eastl::span<const EntityId> selected_ids);

}  // namespace Blunder
