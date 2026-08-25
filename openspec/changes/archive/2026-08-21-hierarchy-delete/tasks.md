## 1. Soft-delete Command label

- [x] 1.1 Add `display_label` + `label()` on `SoftDeleteEntityCommand`; extend `makeSoftDeleteEntityCommand` with optional label (default `Delete Entity`), matching spawn
- [x] 1.2 Snapshot the entity name in `softDeleteSelection` so the Delete key also gets `Delete {name}` / `Delete Entity`
- [x] 1.3 Tests in `editor_commands_test`: named entity → `Delete Cube`; empty name → `Delete Entity`; existing undo/redo tombstone round-trip still passes

## 2. Hierarchy Panel Delete

- [x] 2.1 `hierarchy.slint`: on the entity-row menu only, keep Empty / Camera / Light, add `MenuSeparator`, add `Delete` that fires `delete-requested(entity-id)`; leave scene-header and empty-area menus as three Create items
- [x] 2.2 Wire `hierarchy-delete-requested` on `editor_window.slint` and `floating_panel_window.slint`; bind in `slint_system` and `dock_floating_window_host` (same two sites as Create…)
- [x] 2.3 On Delete: single-select the callback entity id, then `EditorSceneEditSystem::softDeleteSelection()`; no-op when that returns false; sync Hierarchy + Inspector like Create…
- [x] 2.4 Build `engine_editor`

## 3. Same-as-key semantics tests

- [x] 3.1 Test: soft-delete parent with a child — child is omitted from export while parent is tombstoned; one undo restores parent EntityId and the still-parented child (no extra History row)
- [x] 3.2 Test: soft-delete an entity with `isMain` Camera succeeds (no protection)

## 4. Validation

- [x] 4.1 Confirm `CONTEXT.md` **Hierarchy Delete** / **Create…** menu order match shipped copy
- [x] 4.2 Manual: docked + floating — row menu Empty/Camera/Light, separator, Delete; empty area and scene title have no Delete; Delete Cube in History; Ctrl+Z restores parent+children; Delete key still works and uses the same label; Main Camera can be deleted
