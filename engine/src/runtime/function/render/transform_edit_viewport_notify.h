#pragma once

namespace Blunder {

class RenderSystem;
class SlintSystem;

void notifyViewportAfterGizmoTransformEdit(RenderSystem* render_system);
void notifyViewportAfterInspectorTransformEdit(RenderSystem* render_system,
                                               SlintSystem* slint_system);

}  // namespace Blunder
