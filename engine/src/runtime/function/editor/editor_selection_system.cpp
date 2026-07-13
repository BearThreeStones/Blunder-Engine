#include "runtime/function/editor/editor_selection_system.h"

#include "runtime/function/global/global_context.h"
#include "runtime/function/render/gizmo/transform_gizmo_types.h"
#include "runtime/function/render/render_system.h"
#include "runtime/function/slint/slint_system.h"

namespace Blunder {

void EditorSelectionSystem::requestViewportRedraw() {
  if (g_runtime_global_context.m_render_system) {
    g_runtime_global_context.m_render_system->requestViewportRedraw();
  }
}

void EditorSelectionSystem::onSelectionChanged(EntityId primary,
                                               bool activate_translate_gizmo) {
  m_primary_id = primary;
  m_dirty = true;
  if (!g_runtime_global_context.m_render_system) {
    return;
  }
  if (activate_translate_gizmo && isValid(primary)) {
    g_runtime_global_context.m_render_system->setTransformGizmoMode(
        TransformGizmoMode::translate);
  }
  if (g_runtime_global_context.m_slint_system) {
    g_runtime_global_context.m_slint_system->markViewportDirtyRegion();
  }
  requestViewportRedraw();
}

void EditorSelectionSystem::clearSelection() {
  if (m_selected_ids.empty()) {
    return;
  }
  m_selected_ids.clear();
  onSelectionChanged(k_invalid_entity_id, false);
}

void EditorSelectionSystem::setSelection(EntityId id) {
  if (!isValid(id)) {
    clearSelection();
    return;
  }
  if (m_selected_ids.size() == 1 && *m_selected_ids.begin() == id &&
      m_primary_id == id) {
    return;
  }
  m_selected_ids.clear();
  m_selected_ids.insert(id);
  onSelectionChanged(id, true);
}

void EditorSelectionSystem::addToSelection(EntityId id) {
  if (!isValid(id)) {
    return;
  }
  const bool inserted = m_selected_ids.insert(id).second;
  if (!inserted && m_primary_id == id) {
    return;
  }
  onSelectionChanged(id, inserted);
}

void EditorSelectionSystem::removeFromSelection(EntityId id) {
  if (!isValid(id)) {
    return;
  }
  const auto it = m_selected_ids.find(id);
  if (it == m_selected_ids.end()) {
    return;
  }
  m_selected_ids.erase(it);
  EntityId new_primary = k_invalid_entity_id;
  if (!m_selected_ids.empty()) {
    new_primary = m_primary_id;
    if (!isValid(new_primary) || !isSelected(new_primary)) {
      new_primary = *m_selected_ids.begin();
    }
  }
  onSelectionChanged(new_primary, false);
}

void EditorSelectionSystem::toggleSelection(EntityId id) {
  if (!isValid(id)) {
    return;
  }
  if (isSelected(id)) {
    removeFromSelection(id);
  } else {
    addToSelection(id);
  }
}

eastl::vector<EntityId> EditorSelectionSystem::getSelectedIds() const {
  eastl::vector<EntityId> ids;
  ids.reserve(m_selected_ids.size());
  for (EntityId id : m_selected_ids) {
    ids.push_back(id);
  }
  return ids;
}

bool EditorSelectionSystem::isSelected(EntityId id) const {
  if (!isValid(id)) {
    return false;
  }
  return m_selected_ids.find(id) != m_selected_ids.end();
}

}  // namespace Blunder
