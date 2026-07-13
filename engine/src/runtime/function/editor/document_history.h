#pragma once

#include "EASTL/functional.h"
#include "EASTL/unique_ptr.h"
#include "EASTL/vector.h"

#include "runtime/function/scene/entity_id.h"

namespace Blunder {

struct SelectionSnapshot {
  EntityId primary{k_invalid_entity_id};
};

/// Reversible unit on Document History. Caller applies the edit, then push().
class IEditorCommand {
 public:
  virtual ~IEditorCommand() = default;
  virtual void undo() = 0;
  virtual void redo() = 0;

  SelectionSnapshot selection_before{};
  SelectionSnapshot selection_after{};
};

/// Linear document-scoped undo/redo stack (Editor History for one open scene).
class DocumentHistory final {
 public:
  static constexpr size_t k_default_max_depth = 100;

  void setMaxDepth(size_t max_depth);
  size_t maxDepth() const { return m_max_depth; }

  void clear();

  /// Push an already-applied command; truncates redo; may drop oldest.
  void push(eastl::unique_ptr<IEditorCommand> command);

  bool canUndo() const;
  bool canRedo() const;
  bool undo();
  bool redo();

  void markSaveBaseline();
  bool isDirtyRelativeToSave() const;

  void setSelectionRestorer(
      eastl::function<void(const SelectionSnapshot&)> restorer);

 private:
  void restoreSelection(const SelectionSnapshot& snapshot);
  void dropOldestIfNeeded();

  eastl::vector<eastl::unique_ptr<IEditorCommand>> m_commands;
  size_t m_cursor{0};       // next push index; undo moves left
  size_t m_max_depth{k_default_max_depth};
  size_t m_save_baseline{0};
  eastl::function<void(const SelectionSnapshot&)> m_selection_restorer;
};

}  // namespace Blunder
