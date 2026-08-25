#include "runtime/function/editor/document_history.h"

#include <cstdio>

namespace {

int g_failures = 0;

void expect_true(const char* label, bool ok) {
  if (!ok) {
    std::fprintf(stderr, "FAIL %s\n", label);
    ++g_failures;
  }
}

struct CounterCommand final : Blunder::IEditorCommand {
  int* value{nullptr};
  int delta{0};

  void undo() override {
    if (value != nullptr) {
      *value -= delta;
    }
  }

  void redo() override {
    if (value != nullptr) {
      *value += delta;
    }
  }
};

eastl::unique_ptr<CounterCommand> makeCounter(int* value, int delta,
                                              Blunder::EntityId before,
                                              Blunder::EntityId after) {
  auto cmd = eastl::make_unique<CounterCommand>();
  cmd->value = value;
  cmd->delta = delta;
  cmd->selection_before.primary = before;
  cmd->selection_after.primary = after;
  *value += delta;  // already applied before push
  return cmd;
}

Blunder::EntityId g_restored_selection = Blunder::k_invalid_entity_id;

void restoreSelection(const Blunder::SelectionSnapshot& snapshot) {
  g_restored_selection = snapshot.primary;
}

}  // namespace

int main() {
  using namespace Blunder;

  // --- push + undo/redo ---
  {
    DocumentHistory history;
    int value = 0;
    history.push(makeCounter(&value, 3, EntityId{1}, EntityId{2}));
    expect_true("canUndo after push", history.canUndo());
    expect_true("cannot redo initially", !history.canRedo());
    expect_true("value after apply", value == 3);

    expect_true("undo returns true", history.undo());
    expect_true("value after undo", value == 0);
    expect_true("canRedo after undo", history.canRedo());

    expect_true("redo returns true", history.redo());
    expect_true("value after redo", value == 3);
  }

  // --- redo truncated by divergent push ---
  {
    DocumentHistory history;
    int value = 0;
    history.push(makeCounter(&value, 1, k_invalid_entity_id, EntityId{1}));
    history.push(makeCounter(&value, 2, EntityId{1}, EntityId{2}));
    history.undo();
    expect_true("after undo value is 1", value == 1);
    history.push(makeCounter(&value, 10, EntityId{1}, EntityId{3}));
    expect_true("cannot redo after divergent push", !history.canRedo());
    expect_true("value after divergent", value == 11);
    history.undo();
    expect_true("undo divergent leaves 1", value == 1);
  }

  // --- max depth drops oldest ---
  {
    DocumentHistory history;
    history.setMaxDepth(2);
    int value = 0;
    history.push(makeCounter(&value, 1, k_invalid_entity_id, EntityId{1}));
    history.push(makeCounter(&value, 2, EntityId{1}, EntityId{2}));
    history.push(makeCounter(&value, 4, EntityId{2}, EntityId{3}));
    expect_true("value after three pushes", value == 7);
    history.undo();
    expect_true("undo last", value == 3);
    history.undo();
    expect_true("undo second", value == 1);
    expect_true("oldest already dropped", !history.canUndo());
  }

  // --- dirty baseline ---
  {
    DocumentHistory history;
    int value = 0;
    expect_true("clean initially", !history.isDirtyRelativeToSave());
    history.push(makeCounter(&value, 1, k_invalid_entity_id, EntityId{1}));
    expect_true("dirty after edit", history.isDirtyRelativeToSave());
    history.markSaveBaseline();
    expect_true("clean after save baseline", !history.isDirtyRelativeToSave());
    history.push(makeCounter(&value, 2, EntityId{1}, EntityId{2}));
    expect_true("dirty after post-save edit", history.isDirtyRelativeToSave());
    history.undo();
    expect_true("clean after undo to baseline", !history.isDirtyRelativeToSave());
  }

  // --- selection restore ---
  {
    DocumentHistory history;
    g_restored_selection = k_invalid_entity_id;
    history.setSelectionRestorer(&restoreSelection);
    int value = 0;
    history.push(makeCounter(&value, 1, EntityId{10}, EntityId{20}));
    history.undo();
    expect_true("undo restores before selection",
                g_restored_selection == EntityId{10});
    history.redo();
    expect_true("redo restores after selection",
                g_restored_selection == EntityId{20});
  }

  // --- clear ---
  {
    DocumentHistory history;
    int value = 0;
    history.push(makeCounter(&value, 1, k_invalid_entity_id, EntityId{1}));
    history.clear();
    expect_true("cannot undo after clear", !history.canUndo());
    expect_true("clear resets dirty", !history.isDirtyRelativeToSave());
  }

  {
    expect_true(
        "viewport routes document",
        resolveUndoScope(false, false, false) == EditorUndoScope::document);
    expect_true("browser routes global",
                resolveUndoScope(true, false, false) == EditorUndoScope::global);
    expect_true("asset inspector routes global",
                resolveUndoScope(false, false, true) == EditorUndoScope::global);
    expect_true("inline rename claims text",
                resolveUndoScope(true, true, true) == EditorUndoScope::text);
  }

  {
    DocumentHistory document;
    DocumentHistory global;
    int document_value = 0;
    int global_value = 0;
    document.push(makeCounter(&document_value, 1, k_invalid_entity_id, EntityId{1}));
    global.push(makeCounter(&global_value, 5, k_invalid_entity_id, EntityId{1}));
    document.clear();
    expect_true("open-scene style clear leaves global", global.canUndo());
    expect_true("global value untouched by document clear", global_value == 5);
    expect_true("document cannot undo after clear", !document.undo());
    expect_true("viewport undo would not change global", global_value == 5);
    expect_true("global undo", global.undo());
    expect_true("global undone", global_value == 0);
  }

  {
    DocumentHistory history;
    int value = 0;
    history.push(makeCounter(&value, 1, k_invalid_entity_id, EntityId{1}));
    history.push(makeCounter(&value, 2, EntityId{1}, EntityId{2}));
    history.push(makeCounter(&value, 4, EntityId{2}, EntityId{3}));
    expect_true("label default",
                history.commandAt(0) != nullptr &&
                    history.commandAt(0)->label() == "Edit");
    expect_true("jumpTo undoes to first", history.jumpTo(1));
    expect_true("value after jumpTo 1", value == 1);
    expect_true("jumpTo redoes to all", history.jumpTo(3));
    expect_true("value after jumpTo 3", value == 7);
  }

  if (g_failures != 0) {
    std::fprintf(stderr, "%d failure(s)\n", g_failures);
    return 1;
  }
  return 0;
}
