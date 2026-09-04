#include "runtime/function/editor/object_active_ops.h"

#include "runtime/function/editor/document_history_helpers.h"
#include "runtime/function/editor/editor_commands.h"
#include "runtime/function/editor/editor_selection_system.h"
#include "runtime/function/editor/hierarchy_system.h"
#include "runtime/function/global/global_context.h"
#include "runtime/function/render/render_system.h"
#include "runtime/function/scene/scene_instance.h"
#include "runtime/function/ui/ui_host.h"
#include "runtime/function/ui/view_models/editor_panels_view_model.h"

namespace Blunder {
namespace {

eastl::vector<EntityId> currentSelectedIds() {
  if (g_runtime_global_context.m_editor_selection == nullptr) {
    return {};
  }
  return g_runtime_global_context.m_editor_selection->getSelectedIds();
}

}  // namespace

void refreshObjectActiveEditorUi(SceneInstance* scene) {
  if (g_runtime_global_context.m_hierarchy) {
    // treeRows() is a cache; syncHierarchy copies it. Rebuild so Object Active
    // and Active in Hierarchy reach the checkbox / grey name this frame.
    g_runtime_global_context.m_hierarchy->rebuildVisibleTree(scene);
    g_runtime_global_context.m_hierarchy->markDirty();
  }
  if (g_runtime_global_context.m_editor_selection) {
    g_runtime_global_context.m_editor_selection->markDirty();
  }
  if (g_runtime_global_context.m_render_system) {
    g_runtime_global_context.m_render_system->requestViewportRedraw();
  }
  if (g_runtime_global_context.m_ui_host) {
    g_runtime_global_context.m_ui_host->panels().markDirty(
        EditorPanelDirty::hierarchy);
    g_runtime_global_context.m_ui_host->panels().markDirty(
        EditorPanelDirty::inspector);
    g_runtime_global_context.m_ui_host->tickEditorPanels();
  }
}

bool alignedObjectActiveAfter(const SceneInstance& scene,
                              const eastl::vector<EntityId>& ids) {
  if (ids.empty()) {
    return true;
  }
  for (EntityId id : ids) {
    if (!scene.isObjectActive(id)) {
      return true;
    }
  }
  return false;
}

void commitObjectActiveSelection(SceneInstance* scene,
                                 const eastl::vector<EntityId>& ids) {
  if (scene == nullptr || ids.empty()) {
    return;
  }
  const bool after = alignedObjectActiveAfter(*scene, ids);
  eastl::vector<ObjectActiveEntry> entries;
  entries.reserve(ids.size());
  for (EntityId id : ids) {
    if (scene->getEntity(id) == nullptr) {
      continue;
    }
    ObjectActiveEntry entry;
    entry.entity_id = id;
    entry.before = scene->isObjectActive(id);
    entry.after = after;
    scene->setObjectActive(id, after);
    entries.push_back(entry);
  }
  if (entries.empty()) {
    return;
  }
  refreshObjectActiveEditorUi(scene);
  const SelectionSnapshot snap = currentSelectionSnapshot();
  pushDocumentCommand(makeSetObjectActiveCommand(scene, eastl::move(entries),
                                                 snap, snap));
}

void commitHierarchyActiveCheckbox(SceneInstance* scene, EntityId clicked_id) {
  if (scene == nullptr || !isValid(clicked_id) ||
      scene->getEntity(clicked_id) == nullptr) {
    return;
  }
  EditorSelectionSystem* selection = g_runtime_global_context.m_editor_selection.get();
  if (selection != nullptr && selection->isSelected(clicked_id) &&
      selection->getSelectedIds().size() > 1) {
    commitObjectActiveSelection(scene, selection->getSelectedIds());
    return;
  }
  if (selection != nullptr) {
    selection->setSelection(clicked_id);
  }
  ObjectActiveEntry entry;
  entry.entity_id = clicked_id;
  entry.before = scene->isObjectActive(clicked_id);
  entry.after = !entry.before;
  scene->setObjectActive(clicked_id, entry.after);
  refreshObjectActiveEditorUi(scene);
  const SelectionSnapshot snap = currentSelectionSnapshot();
  eastl::vector<ObjectActiveEntry> entries;
  entries.push_back(entry);
  pushDocumentCommand(
      makeSetObjectActiveCommand(scene, eastl::move(entries), snap, snap));
}

void commitInspectorActiveCheckbox(SceneInstance* scene) {
  commitObjectActiveSelection(scene, currentSelectedIds());
}

void commitHierarchyActiveToggle(SceneInstance* scene) {
  commitObjectActiveSelection(scene, currentSelectedIds());
}

}  // namespace Blunder
