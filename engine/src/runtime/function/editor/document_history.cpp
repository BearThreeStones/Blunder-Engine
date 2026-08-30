#include "runtime/function/editor/document_history.h"

namespace Blunder {

void DocumentHistory::setMaxDepth(size_t max_depth) {
  m_max_depth = max_depth == 0 ? 1 : max_depth;
  while (m_commands.size() > m_max_depth) {
    m_commands.erase(m_commands.begin());
    if (m_cursor > 0) {
      --m_cursor;
    }
    if (m_save_baseline > 0) {
      --m_save_baseline;
    } else {
      // Baseline fell off the stack — treat as divergent from save.
      m_save_baseline = static_cast<size_t>(-1);
    }
  }
}

void DocumentHistory::clear() {
  m_commands.clear();
  m_cursor = 0;
  m_save_baseline = 0;
}

void DocumentHistory::push(eastl::unique_ptr<IEditorCommand> command) {
  if (!command) {
    return;
  }

  if (m_cursor < m_commands.size()) {
    m_commands.resize(m_cursor);
  }

  m_commands.push_back(eastl::move(command));
  m_cursor = m_commands.size();
  dropOldestIfNeeded();
  if (m_after_mutation && m_cursor > 0) {
    m_after_mutation(*m_commands[m_cursor - 1]);
  }
}

bool DocumentHistory::canUndo() const { return m_cursor > 0; }

bool DocumentHistory::canRedo() const { return m_cursor < m_commands.size(); }

bool DocumentHistory::undo() {
  if (!canUndo()) {
    return false;
  }
  --m_cursor;
  IEditorCommand* command = m_commands[m_cursor].get();
  command->undo();
  restoreSelection(command->selection_before);
  if (m_after_mutation) {
    m_after_mutation(*command);
  }
  return true;
}

bool DocumentHistory::redo() {
  if (!canRedo()) {
    return false;
  }
  IEditorCommand* command = m_commands[m_cursor].get();
  command->redo();
  restoreSelection(command->selection_after);
  ++m_cursor;
  if (m_after_mutation) {
    m_after_mutation(*command);
  }
  return true;
}

const IEditorCommand* DocumentHistory::commandAt(size_t index) const {
  if (index >= m_commands.size()) {
    return nullptr;
  }
  return m_commands[index].get();
}

bool DocumentHistory::jumpTo(size_t applied_count) {
  if (applied_count > m_commands.size()) {
    return false;
  }
  while (m_cursor > applied_count) {
    if (!undo()) {
      return false;
    }
  }
  while (m_cursor < applied_count) {
    if (!redo()) {
      return false;
    }
  }
  return true;
}

EditorUndoScope resolveUndoScope(bool content_browser_focused,
                                 bool inline_rename_active,
                                 bool asset_inspector_focused,
                                 bool attachment_preview_focused) {
  if (inline_rename_active) {
    return EditorUndoScope::text;
  }
  if (attachment_preview_focused) {
    return EditorUndoScope::document;
  }
  if (content_browser_focused || asset_inspector_focused) {
    return EditorUndoScope::global;
  }
  return EditorUndoScope::document;
}

void DocumentHistory::markSaveBaseline() { m_save_baseline = m_cursor; }

bool DocumentHistory::isDirtyRelativeToSave() const {
  return m_cursor != m_save_baseline;
}

void DocumentHistory::setSelectionRestorer(
    eastl::function<void(const SelectionSnapshot&)> restorer) {
  m_selection_restorer = eastl::move(restorer);
}

void DocumentHistory::setAfterMutationObserver(
    eastl::function<void(const IEditorCommand&)> observer) {
  m_after_mutation = eastl::move(observer);
}

void DocumentHistory::restoreSelection(const SelectionSnapshot& snapshot) {
  if (m_selection_restorer) {
    m_selection_restorer(snapshot);
  }
}

void DocumentHistory::dropOldestIfNeeded() {
  while (m_commands.size() > m_max_depth) {
    m_commands.erase(m_commands.begin());
    if (m_cursor > 0) {
      --m_cursor;
    }
    if (m_save_baseline == 0) {
      // Saved state is no longer on the stack.
      m_save_baseline = static_cast<size_t>(-1);
    } else if (m_save_baseline != static_cast<size_t>(-1)) {
      --m_save_baseline;
    }
  }
}

}  // namespace Blunder
