#pragma once

#include <cstdint>

#include "EASTL/hash_set.h"
#include "EASTL/string.h"
#include "EASTL/vector.h"

#include "runtime/function/editor/editor_selection_system.h"
#include "runtime/function/editor/hierarchy_types.h"
#include "runtime/function/scene/entity_id.h"

namespace Blunder {

class SceneInstance;
class SceneSystem;

class HierarchySystem final {
 public:
  void clearExpanded();
  void rebuildVisibleTree(SceneInstance* scene_instance);
  const eastl::vector<EditorHierarchyTreeRow>& treeRows() const { return m_tree_rows; }

  void toggleExpanded(EntityId entity_id);
  bool isExpanded(EntityId entity_id) const;

  /// Row index under the hierarchy tree band, or -1 if none.
  int32_t hitTestRow(float logical_x, float logical_y, int32_t tree_origin_y,
                     float row_pitch) const;

  /// Select entity from window coordinates; returns true if a row was hit.
  bool selectEntityAt(float window_x, float window_y, int32_t hierarchy_origin_x,
                      int32_t tree_origin_y, float panel_width,
                      float panel_height, float row_pitch,
                      EditorSelectionSystem& selection);

  bool isDirty() const { return m_dirty; }
  void clearDirty() { m_dirty = false; }
  void markDirty() { m_dirty = true; }

 private:
  void appendVisibleSubtree(SceneInstance* scene_instance, EntityId entity_id,
                            int32_t depth);

  eastl::vector<EditorHierarchyTreeRow> m_tree_rows;
  eastl::hash_set<EntityId> m_expanded_entity_ids;
  bool m_dirty{true};
};

}  // namespace Blunder
