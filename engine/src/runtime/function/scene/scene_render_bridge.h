#pragma once

namespace Blunder {

class RenderSystem;
class SceneInstance;

/// Rebuilds the render draw list from mesh renderers on a scene instance.
void syncSceneToRender(RenderSystem* render_system, SceneInstance* scene_instance);

}  // namespace Blunder
