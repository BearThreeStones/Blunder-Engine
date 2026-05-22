#pragma once

#include "runtime/function/scene/entity_id.h"

namespace Blunder {

/// Tracks the currently selected scene entity in the editor.
class EditorSelectionSystem final {
 public:
  void clearSelection();
  void setSelection(EntityId id);
  EntityId getSelection() const { return m_selected_id; }
  bool hasSelection() const { return isValid(m_selected_id); }

  bool isDirty() const { return m_dirty; }
  void clearDirty() { m_dirty = false; }
  void markDirty() { m_dirty = true; }

 private:
  EntityId m_selected_id{k_invalid_entity_id};
  bool m_dirty{false};
};

}  // namespace Blunder
