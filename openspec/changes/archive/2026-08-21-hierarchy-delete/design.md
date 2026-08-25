## Context

See proposal.md for motivation. Hierarchy rows already open a Create… `ContextMenuArea` (`Empty` / `Camera` / `Light`) after right-click single-select (`hierarchy.slint`). Empty area and scene title use the same three items with parent id 0. Scene delete already exists: Delete key → `EditorSceneEditSystem::softDeleteSelection()` → `makeSoftDeleteEntityCommand`. That command does not override `label()`, so History shows `Edit`. `IEditorCommand` default label is `Edit`. Spawn already takes a display label (`Create {name}`).

## Goals / Non-Goals

**Goals:**

- Add `Delete` to the entity-row menu only, after a `MenuSeparator`.
- Dispatch that item into the existing soft-delete command path (same tombstone, selection, undo as the Delete key).
- Snapshot a `Delete {name}` / `Delete Entity` label on that command for both callers.

**Non-Goals:**

- New delete semantics, confirm dialogs, or Main Camera guards.
- Duplicate / Rename.
- Changing empty-area or scene-title menus.
- A second History command per descendant.

## Decisions

### D1 — Dispatch through `softDeleteSelection`
**Choice:** After the row right-click has already single-selected the entity, Hierarchy Delete calls the same `softDeleteSelection()` the Delete key uses (or a thin wrapper that re-selects the callback entity id then calls it). Rebuild Hierarchy / Inspector the same way Create… does after apply.
**Why:** Grilled: menu and key are one operation. Avoid a second apply path that could drift.
**Rejected:** A new `applyHierarchyDelete` that re-implements tombstone; calling `softDeleteEntity` without going through the existing selection/history helper.

### D2 — Separate Slint callback, not a Create… kind
**Choice:** Add `delete-requested(int)` on `HierarchyPanel` and `hierarchy-delete-requested(int)` on docked/floating windows. Do not add a `delete` value to `HierarchyCreateKind`.
**Why:** Create kinds are spawn catalog; Delete is a different operation. Keeps `parseHierarchyCreateKind` closed.
**Rejected:** `create-requested(id, "delete")`.

### D3 — Label on `SoftDeleteEntityCommand` like spawn
**Choice:** Add `display_label` (default `Delete Entity`) and `label()` override. `makeSoftDeleteEntityCommand` takes an optional label. Callers snapshot `entity->getName()` at push time (the entity still exists after tombstone). Empty name → `Delete Entity`. Both `softDeleteSelection` and the Hierarchy callback use this factory.
**Why:** Matches `SpawnEntityCommand` / Command label glossary. One command type for menu and key.
**Rejected:** A second command class; leaving `Edit`; live-resolving the name later.

### D4 — Empty / title menus unchanged
**Choice:** Put `MenuSeparator` + `Delete` only in the row `ContextMenuArea`. Scene-header and empty-tree menus stay three Create items.
**Why:** Grilled: those targets have no entity to delete.
**Rejected:** Disabled Delete on empty/title; a viewport Delete.

### D5 — No-op without scene or selection
**Choice:** Keep `softDeleteSelection()` early-outs (no selection, no `SceneInstance`, already tombstoned). Menu may still show Delete on a row; activation with no active scene is a no-op.
**Why:** Same as Create… with no scene; Hierarchy chrome can show `(No Scene)`.
**Rejected:** Crashing; auto-opening a scene.

## Risks / Trade-offs

- [Row menu is a 1×1 `ContextMenuArea` shown from `pointer-event`] → Add items inside that existing menu; do not wrap a second `ContextMenuArea` on the row.
- [Floating panel callback missed] → Wire `hierarchy-delete-requested` on both `editor_window.slint` and `floating_panel_window.slint`, same as Create….
- [Label still `Edit` if only the menu path sets it] → Set the label inside `makeSoftDeleteEntityCommand` / `softDeleteSelection` so the Delete key is covered.
- [Tests that assert default `Edit`] → Update or add assertions for `Delete {name}`.

## Migration Plan

No scene file format change. Rollback: remove the row Delete item and callback; unused label override is harmless if the factory still defaults.

## Open Questions

None — grilled into `CONTEXT.md` **Hierarchy Delete** / **Create…**.
