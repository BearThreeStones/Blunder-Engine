#pragma once

#include <glm/mat4x4.hpp>

#include "EASTL/shared_ptr.h"

namespace Blunder {

class MeshAsset;
class RenderSystem;
class SceneInstance;

/// Rebuilds the render draw list from mesh renderers on a scene instance.
void syncSceneToRender(RenderSystem* render_system, SceneInstance* scene_instance);

/// Bind-pose MeshRenderer draw (Placement Preview / no Entity).
void submitStandaloneMeshToRender(RenderSystem* render_system,
                                  const eastl::shared_ptr<MeshAsset>& mesh,
                                  const glm::mat4& world_matrix);

}  // namespace Blunder
