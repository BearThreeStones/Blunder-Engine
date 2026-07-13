#include "runtime/function/editor/document_history_helpers.h"

#include "runtime/function/editor/editor_scene_edit_system.h"
#include "runtime/function/editor/editor_selection_system.h"
#include "runtime/function/global/global_context.h"

namespace Blunder {

void pushDocumentCommand(eastl::unique_ptr<IEditorCommand> command) {
  if (!command || !g_runtime_global_context.m_document_history) {
    return;
  }
  g_runtime_global_context.m_document_history->push(eastl::move(command));
  if (g_runtime_global_context.m_editor_scene_edit) {
    g_runtime_global_context.m_editor_scene_edit->markDirty();
  }
}

SelectionSnapshot currentSelectionSnapshot() {
  SelectionSnapshot snapshot;
  if (g_runtime_global_context.m_editor_selection &&
      g_runtime_global_context.m_editor_selection->hasSelection()) {
    snapshot.primary = g_runtime_global_context.m_editor_selection->getSelection();
  }
  return snapshot;
}

}  // namespace Blunder
