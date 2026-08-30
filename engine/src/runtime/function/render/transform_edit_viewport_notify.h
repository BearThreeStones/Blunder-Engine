#pragma once

namespace Blunder {

class RenderSystem;
class SlintSystem;

void notifyViewportAfterGizmoTransformEdit(RenderSystem* render_system);
void notifyViewportAfterInspectorTransformEdit(RenderSystem* render_system,
                                               SlintSystem* slint_system);
void notifyViewportAfterInspectorLightEdit(RenderSystem* render_system,
                                           SlintSystem* slint_system);

/// Animation preview / cine pose advanced with a static camera: same skip-gate
/// contract as Inspector transform apply.
void notifyViewportAfterAnimationPreviewFrame(RenderSystem* render_system,
                                              SlintSystem* slint_system);

}  // namespace Blunder
