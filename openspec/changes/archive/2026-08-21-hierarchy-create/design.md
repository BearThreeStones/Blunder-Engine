## Context

See proposal.md for motivation. Hierarchy Panel today is left-click select / chevron expand only (`hierarchy.slint`). Content Browser already uses Slint `ContextMenuArea` + `MenuItem`. Spawn uses `SceneInstance::createEntity` (identity TRS by default, optional parent) plus `makeSpawnEntityCommand` (undo = `softDeleteEntity`). Inspector **Add…** Camera/Light uses `applyInspectorUniqueAdd` — Camera currently sets `isMain` when the scene has no Main; Create… Camera must not use that path as-is.

## Goals / Non-Goals

**Goals:**

- One Hierarchy `ContextMenuArea` path (docked + floating) for Empty / Camera / Light.
- One Document History step: spawn + optional Unique + selection.
- Unique names against `SceneInstance` name map (`findEntityByName` / light linking).

**Non-Goals:**

- Viewport or top-bar Create….
- Reusing `applyInspectorUniqueAdd` for Camera (would auto-Main).
- Putting expand/collapse into Document History.
- Duplicate / Rename / Delete on this menu.

## Decisions

### D1 — Slint `ContextMenuArea` like Content Browser
**Choice:** Wrap each Hierarchy row, the scene-header, and the empty tree viewport with `ContextMenuArea` + three `MenuItem`s (`Empty` / `Camera` / `Light`). Callback carries parent `entity-id` (0 / invalid = root). Right-down on a row also fires `entity-selected` (single) before/as the menu opens. Chevron `TouchArea` stays left-only for toggle.
**Why:** Same widget as Browser; parent is the click target, not “whatever was selected last frame.”
**Rejected:** Inspector Add… popup on the row; a native OS menu outside Slint.

### D2 — Spawn command covers Unique undo
**Choice:** `createEntity` + `setCamera` / `setLight` (or neither for Empty), then `makeSpawnEntityCommand` with before/after selection. Soft-delete already hides the entity and its Unique maps. Override `label()` to `Create Empty` / `Create Camera` / `Create Light` (entity name in the label when known).
**Why:** Spec is one undo; Unique lives on the entity; no second Add… command.
**Rejected:** Spawn then `makeAddUniqueAttachmentCommand`; a new command class that re-implements tombstone.

### D3 — Camera Unique without Add… auto-Main
**Choice:** `CameraComponent` defaults with `is_main = false`. `LightComponent{}` (Directional). Do not call `applyInspectorUniqueAdd` for Create… Camera.
**Why:** Grilled: Create Camera never marks Main. Add… Camera still auto-Mains when none exists.
**Rejected:** Sharing Add… Camera blindly; flipping Main if the scene has none.

### D4 — Scene-wide unique names
**Choice:** Start from `Empty` / `Camera` / `Light`; while `findEntityByName` hits (and the entity is in the document), append `_1`, `_2`, ….
**Why:** `m_name_to_id` is one-name-one-id; Light linking persists names.
**Rejected:** Sibling-only uniqueness; silent overwrite.

### D5 — Expand parent is chrome
**Choice:** After Create…, `HierarchySystem` expands the parent id if present. Undo does not re-collapse.
**Why:** Selection is already in the Spawn command snapshot; expand is view state.
**Rejected:** Recording expand in Document History.

### D6 — No active scene
**Choice:** Menu may still open; choosing an item with no active `SceneInstance` is a no-op (no command).
**Why:** Hierarchy chrome exists with `(No Scene)`.
**Rejected:** Crashing; creating a scene from this menu.

## Risks / Trade-offs

- [Nested chevron `TouchArea` eats right-click] → Keep chevron left-only; put `ContextMenuArea` on the full row so right-click still selects + menu.
- [Add… Camera vs Create… Camera Main policy] → Separate apply path (D3); tests must not call Add… helper for Create Camera.
- [Duplicate names vs light linking] → D4 scene-wide `_1`.
- [Floating panel callback not wired] → Same `hierarchy-create-requested` on `editor_window.slint` and `floating_panel_window.slint`.

## Migration Plan

No scene file format change. Existing scenes unchanged. Rollback: revert Hierarchy Slint menu; unused create helper is dead.

## Open Questions

None — grilled into `CONTEXT.md` **Create…** / **Create… command**.
