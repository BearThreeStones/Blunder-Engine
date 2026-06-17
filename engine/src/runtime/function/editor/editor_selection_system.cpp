#include "runtime/function/editor/editor_selection_system.h"

#include "runtime/function/global/global_context.h"
#include "runtime/function/render/gizmo/transform_gizmo_types.h"
#include "runtime/function/render/render_system.h"

namespace Blunder {

void EditorSelectionSystem::clearSelection() {
  if (m_selected_id != k_invalid_entity_id) {
    m_selected_id = k_invalid_entity_id;
    m_dirty = true;
    if (g_runtime_global_context.m_render_system) {
      g_runtime_global_context.m_render_system->requestViewportRedraw();
    }
  }
}

void EditorSelectionSystem::setSelection(EntityId id) {
  if (m_selected_id == id) {
    return;
  }
  m_selected_id = id;
  m_dirty = true;
  if (!g_runtime_global_context.m_render_system) {
    return;
  }
  if (isValid(id)) {
    g_runtime_global_context.m_render_system->setTransformGizmoMode(
        TransformGizmoMode::translate);
  }
  g_runtime_global_context.m_render_system->requestViewportRedraw();
}

}  // namespace Blunder
