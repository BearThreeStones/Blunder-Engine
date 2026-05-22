#include "runtime/function/editor/hierarchy_system.h"

#include "runtime/function/scene/entity.h"
#include "runtime/function/scene/scene_instance.h"

namespace Blunder {

void HierarchySystem::clearExpanded() { m_expanded_entity_ids.clear(); }

void HierarchySystem::rebuildVisibleTree(SceneInstance* scene_instance) {
  m_tree_rows.clear();
  if (scene_instance == nullptr) {
    m_dirty = false;
    return;
  }

  if (m_expanded_entity_ids.empty()) {
    scene_instance->forEachEntity([&](EntityId entity_id, const Entity&) {
      bool has_children = false;
      scene_instance->forEachChild(entity_id, [&](EntityId, const Entity&) {
        has_children = true;
      });
      if (has_children) {
        m_expanded_entity_ids.insert(entity_id);
      }
    });
  }

  scene_instance->forEachEntity([&](EntityId entity_id, const Entity& entity) {
    if (!isValid(entity.getParentId())) {
      appendVisibleSubtree(scene_instance, entity_id, 0);
    }
  });

  m_dirty = false;
}

void HierarchySystem::appendVisibleSubtree(SceneInstance* scene_instance,
                                           EntityId entity_id, int32_t depth) {
  const Entity* entity = scene_instance->getEntity(entity_id);
  if (entity == nullptr) {
    return;
  }

  bool has_children = false;
  scene_instance->forEachChild(entity_id, [&](EntityId, const Entity&) {
    has_children = true;
  });

  const bool expanded = !has_children || isExpanded(entity_id);

  EditorHierarchyTreeRow row;
  row.entity_id = entity_id;
  row.display_name = entity->getName();
  row.depth = depth;
  row.is_expanded = expanded;
  row.has_children = has_children;
  m_tree_rows.push_back(eastl::move(row));

  if (!has_children || !expanded) {
    return;
  }

  scene_instance->forEachChild(entity_id, [&](EntityId child_id, const Entity&) {
    appendVisibleSubtree(scene_instance, child_id, depth + 1);
  });
}

void HierarchySystem::toggleExpanded(EntityId entity_id) {
  if (!isValid(entity_id)) {
    return;
  }
  if (m_expanded_entity_ids.find(entity_id) != m_expanded_entity_ids.end()) {
    m_expanded_entity_ids.erase(entity_id);
  } else {
    m_expanded_entity_ids.insert(entity_id);
  }
  m_dirty = true;
}

bool HierarchySystem::isExpanded(EntityId entity_id) const {
  if (!isValid(entity_id)) {
    return false;
  }
  return m_expanded_entity_ids.find(entity_id) != m_expanded_entity_ids.end();
}

int32_t HierarchySystem::hitTestRow(float logical_x, float logical_y,
                                    int32_t tree_origin_y,
                                    float row_pitch) const {
  (void)logical_x;
  if (logical_y < static_cast<float>(tree_origin_y)) {
    return -1;
  }
  const float relative_y = logical_y - static_cast<float>(tree_origin_y);
  const int32_t index = static_cast<int32_t>(relative_y / row_pitch);
  if (index < 0 || static_cast<size_t>(index) >= m_tree_rows.size()) {
    return -1;
  }
  return index;
}

bool HierarchySystem::selectEntityAt(float window_x, float window_y,
                                     int32_t hierarchy_origin_x,
                                     int32_t tree_origin_y, float panel_width,
                                     float panel_height, float row_pitch,
                                     EditorSelectionSystem& selection) {
  const bool in_panel_x =
      window_x >= static_cast<float>(hierarchy_origin_x) &&
      window_x < static_cast<float>(hierarchy_origin_x) + panel_width;
  const bool in_panel_y =
      window_y >= static_cast<float>(tree_origin_y) &&
      window_y < static_cast<float>(tree_origin_y) + panel_height;
  if (!in_panel_x || !in_panel_y) {
    return false;
  }

  const int32_t row_index = hitTestRow(window_x, window_y, tree_origin_y, row_pitch);
  if (row_index < 0) {
    return false;
  }

  const EntityId entity_id =
      static_cast<EntityId>(m_tree_rows[static_cast<size_t>(row_index)].entity_id);
  if (!isValid(entity_id)) {
    return false;
  }

  selection.setSelection(entity_id);
  return true;
}

}  // namespace Blunder
