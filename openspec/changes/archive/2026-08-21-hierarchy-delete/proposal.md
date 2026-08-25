## Why

Hierarchy Create… shipped without Delete on the same row menu. Authors already scene-delete with the Delete key, but the row menu cannot. Soft-delete History still shows `Edit` instead of the Command label `Delete {name}`.

## What Changes

- Entity-row right-click menu (docked + floating): keep `Empty` / `Camera` / `Light`, add a separator, then **Delete**
- Empty Hierarchy area and scene title chrome stay Create-only (no Delete, no trailing separator)
- Hierarchy Delete is the same scene-entity operation as the Delete key: one Document History Command, soft-delete of the right-clicked entity only; descendants stay parented and leave the editable document while the ancestor is tombstoned; undo one step restores that entity (children return)
- No confirm dialog; Create… Camera / Main Camera is not protected; after delete, selection is cleared
- Command label for this command (menu path and Delete key) is `Delete {name}`, or `Delete Entity` when the name is unknown
- Delete key stays; no viewport or top-bar Delete in this slice

**Out of scope:** Duplicate / Rename; asset Delete / Delete Folder; Inspector Remove; a second Command per descendant; disabling the Delete key; a different delete semantic for the menu vs the key

## Capabilities

### New Capabilities

- `hierarchy-delete`: Hierarchy Panel Delete gesture, host (row vs empty/title), same-as-key soft-delete semantics, no confirm, selection after delete

### Modified Capabilities

- `hierarchy-create`: first-slice row menu is no longer Create-only; Duplicate / Rename stay off; empty-area and scene-title menus stay three Create items
- `scene-edit-commands`: soft-delete Command label is `Delete {name}` / `Delete Entity`; Hierarchy Delete and the Delete key share that one command type

## Impact

- `hierarchy.slint` row `ContextMenuArea` only (not scene-header / empty-area menus)
- Docked + floating Hierarchy callbacks (`editor_window.slint`, `floating_panel_window.slint`) plus `slint_system` dispatch into existing `EditorSceneEditSystem::softDeleteSelection()`
- `SoftDeleteEntityCommand::label()` (today defaults to `Edit`) for both the menu path and the Delete key
- Tests: command label; menu path uses the same soft-delete as the key; Main Camera can be deleted; subtree omitted with ancestor and restored on one undo
- Glossary already updated in `CONTEXT.md` (**Hierarchy Delete**, **Create…** menu order)
