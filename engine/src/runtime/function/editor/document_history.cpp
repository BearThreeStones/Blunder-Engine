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
  return true;
}

void DocumentHistory::markSaveBaseline() { m_save_baseline = m_cursor; }

bool DocumentHistory::isDirtyRelativeToSave() const {
  return m_cursor != m_save_baseline;
}

void DocumentHistory::setSelectionRestorer(
    eastl::function<void(const SelectionSnapshot&)> restorer) {
  m_selection_restorer = eastl::move(restorer);
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
