#pragma once

#include "EASTL/unordered_set.h"
#include "EASTL/vector.h"

#include "runtime/function/scene/entity_id.h"

namespace Blunder {

/// Tracks the currently selected scene entities in the editor.
class EditorSelectionSystem final {
 public:
  void clearSelection();
  void setSelection(EntityId id);
  void addToSelection(EntityId id);
  void removeFromSelection(EntityId id);
  void toggleSelection(EntityId id);

  EntityId getPrimarySelection() const { return m_primary_id; }
  EntityId getSelection() const { return getPrimarySelection(); }
  eastl::vector<EntityId> getSelectedIds() const;
  bool isSelected(EntityId id) const;
  bool hasSelection() const { return !m_selected_ids.empty(); }

  bool isDirty() const { return m_dirty; }
  void clearDirty() { m_dirty = false; }
  void markDirty() { m_dirty = true; }

 private:
  void requestViewportRedraw();
  void onSelectionChanged(EntityId primary, bool activate_translate_gizmo);

  eastl::unordered_set<EntityId> m_selected_ids;
  EntityId m_primary_id{k_invalid_entity_id};
  bool m_dirty{false};
};

}  // namespace Blunder
