## Why

Hierarchy is select/expand only. Authors who want a new Light or Camera must spawn or pick an existing entity, then Inspector **Add…**. That is the wrong gesture for “make a child that already has the Unique.” Content Browser already has a right-click Create path; Hierarchy does not.

## What Changes

- Hierarchy Panel right-click **Create…**: spawn a **new entity** (Unity Create semantics), not Inspector **Add…** on the clicked row
- Parent is the right-clicked row; empty area, scene title, and empty scene create a Scene Tree root
- Right-click anywhere on a row (name, Hierarchy Line gutter, expand chevron) single-selects that entity then opens the menu; left-click on the chevron still expands
- First slice: flat menu `Empty` / `Camera` / `Light` only (no Create submenu; no Duplicate / Rename / Delete; no Mesh; no Skeleton / AnimationPlayer / AnimationTree)
- Names `Empty` / `Camera` / `Light` with scene-wide `_1`, `_2` collision; identity local TRS; Create Camera does not mark **Main Camera**; Create Light is one row, default **Directional Light**
- After Create…, the new entity is selected; a collapsed parent expands
- One **Create… command** on Document History (entity + optional Unique); not Spawn then Add…
- Host: Hierarchy Panel only (docked and floating). Not viewport, not editor top bar

**Out of scope:** viewport or top-bar Create…; four Light kinds as menu rows; first-slice animation-host Create…; Mesh spawn on this menu; Duplicate / Rename / Delete; multi-select batch Create; naming dialog

## Capabilities

### New Capabilities

- `hierarchy-create`: Hierarchy Panel Create… gesture, parent/root rules, first-slice catalog, names, TRS, Main Camera policy, selection after Create, menu hit target

### Modified Capabilities

- `hierarchy-panel`: left vs right pointer on rows (select vs select+menu; chevron left-click still expands)
- `scene-edit-commands`: Create… is one Document History Command (spawn + optional Unique + selection)

## Impact

- `hierarchy.slint` + docked/floating Hierarchy callbacks in editor window / floating panel
- Editor Commands factory for Create Empty / Camera / Light (parent, identity TRS, Unique, name collision, selection snapshot)
- `SceneInstance` spawn + Camera/Light Unique (reuse Add… Light/Camera defaults; Camera `isMain` false)
- Tests: parent vs root, names/`_1`, identity TRS, Camera not Main, one undo, Light Directional, selection after Create
- Glossary already updated in `CONTEXT.md` (**Create…**, **Create… command**)
