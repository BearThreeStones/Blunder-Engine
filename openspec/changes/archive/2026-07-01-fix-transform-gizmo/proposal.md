## Why

The editor transform gizmo is partially broken: translate mode shows plane handles but not axis arrows; rotate and scale modes appear empty; and switching the selected entity always resets the gizmo to Move. These issues block basic scene editing and make the transform toolbar unreliable.

## What Changes

- **P0 — Preserve gizmo mode on selection change (option A):** Remove the forced `translate` reset in `EditorSelectionSystem::setSelection`. The active transform mode (Move / Rotate / Scale / none) persists when the user selects a different entity, matching Blender-style behavior.
- **P1 — Fix translate arrow rendering:** Repair the `GizmoDrawStyle::arrow` draw path in `transform_gizmo.slang` / overlay so X/Y/Z axis arrows render correctly on top of plane handles.
- **P2 — Complete scale gizmo:** Add scale-specific visuals (cube/box handles), picking, and drag logic so Scale mode is interactive—not a reuse of broken translate arrows with no mouse handling.

Rotate mode drawing and interaction already exist; verify dial visibility after P0/P1 fixes. No change to rotate math unless verification finds a separate bug.

## Capabilities

### New Capabilities

- `transform-gizmo`: Editor viewport transform manipulator—mode persistence, translate/rotate/scale visuals, picking, and drag behavior.

### Modified Capabilities

_(none — no existing main specs)_

## Impact

- `engine/src/runtime/function/editor/editor_selection_system.cpp` — remove mode reset on selection
- `engine/src/runtime/function/render/gizmo/` — overlay, controller, pick, types, math
- `engine/shaders/transform_gizmo.slang` — arrow (and possibly scale) procedural geometry
- `engine/src/runtime/function/slint/transform_toolbar.slint` — toolbar sync unchanged; benefits from preserved mode
- `engine/src/runtime/function/render/render_system.cpp` — `setTransformGizmoMode` / redraw paths (unchanged API)
