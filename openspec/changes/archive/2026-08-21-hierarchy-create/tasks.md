## 1. Create… apply helper

- [x] 1.1 Unique display name helper: `Empty` / `Camera` / `Light` with scene-wide `_1`, `_2`, … (skip tombstoned names)
- [x] 1.2 Apply Create…: `createEntity` with identity local TRS, parent or root, then LightComponent default Directional or Camera with `isMain` false — do not call `applyInspectorUniqueAdd` for Camera
- [x] 1.3 No-op when there is no active SceneInstance
- [x] 1.4 Tests: child parent; empty-area/title root; name collision `Light_1`; Camera not Main and existing Main unchanged; Light type Directional; identity local TRS; Empty has no Unique

## 2. Document History

- [x] 2.1 After apply, push one `makeSpawnEntityCommand` with selection before/after (new entity selected); override label `Create Empty` / `Create Camera` / `Create Light`
- [x] 2.2 Expand the parent in `HierarchySystem` after Create… (not part of the Command)
- [x] 2.3 Tests: undo Create Light removes entity and restores selection in one step; redo restores same EntityId + Light; undo Create Empty is one step

## 3. Hierarchy Panel UI

- [x] 3.1 `hierarchy.slint`: `ContextMenuArea` on rows, scene-header, and empty tree area; flat `Empty` / `Camera` / `Light`; right-click row single-selects; chevron left-click still toggles, right-click does not expand
- [x] 3.2 Wire `hierarchy-create-requested` (parent id + kind) on `editor_window.slint` and `floating_panel_window.slint`; `slint_system` dispatch
- [x] 3.3 Build `engine_editor`

## 4. Validation

- [x] 4.1 Confirm `CONTEXT.md` **Create…** / **Create… command** match shipped menu copy
- [x] 4.2 Manual: docked + floating Hierarchy — Create Empty/Camera/Light as child; empty area root; Ctrl+Z one step; Inspector shows Camera/Light; New Scene Main Camera stays Main
