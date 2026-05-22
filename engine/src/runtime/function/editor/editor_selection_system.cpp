#include "runtime/function/editor/editor_selection_system.h"

namespace Blunder {

void EditorSelectionSystem::clearSelection() {
  if (m_selected_id != k_invalid_entity_id) {
    m_selected_id = k_invalid_entity_id;
    m_dirty = true;
  }
}

void EditorSelectionSystem::setSelection(EntityId id) {
  if (m_selected_id == id) {
    return;
  }
  m_selected_id = id;
  m_dirty = true;
}

}  // namespace Blunder
