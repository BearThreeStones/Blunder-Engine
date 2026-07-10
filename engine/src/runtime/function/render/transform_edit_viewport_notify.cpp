#include "runtime/function/render/transform_edit_viewport_notify.h"

#include "runtime/function/render/render_system.h"
#include "runtime/function/slint/slint_system.h"

namespace Blunder {

void notifyViewportAfterGizmoTransformEdit(RenderSystem* render_system) {
  if (render_system != nullptr) {
    render_system->requestViewportRedraw();
  }
}

void notifyViewportAfterInspectorTransformEdit(RenderSystem* render_system,
                                               SlintSystem* slint_system) {
  if (slint_system != nullptr) {
    slint_system->markViewportDirtyRegion();
  }
  if (render_system != nullptr) {
    render_system->requestViewportRedraw();
  }
}

}  // namespace Blunder
