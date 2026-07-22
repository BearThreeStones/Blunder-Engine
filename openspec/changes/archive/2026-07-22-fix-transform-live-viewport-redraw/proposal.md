## Why

Transform edits already update the selected entity and Inspector fields, but the 3D viewport mesh and gizmo overlays stay frozen until the camera moves. Gizmo drag and Inspector TRS apply never call `requestViewportRedraw()`, so `tickVulkan` keeps skipping the offscreen pass when camera matrices are unchanged.

## What Changes

- Request a viewport redraw whenever a gizmo transform edit dirties the scene (`markSceneDirty` → `notifyViewportAfterGizmoTransformEdit`).
- On Inspector TRS apply, mark the Slint viewport dirty region and request a viewport redraw (`notifyViewportAfterInspectorTransformEdit`).
- Add small, unit-testable notify helpers so gizmo and Inspector paths share one redraw contract.
- Document the transform-edit → redraw contract next to the existing selection present path in `docs/agents/render-pipeline.md`.

## Capabilities

### New Capabilities

- _(none)_

### Modified Capabilities

- `editor-viewport-pacing`: Require that gizmo drag and Inspector transform edits force a viewport redraw under a static camera (same `requestViewportRedraw` contract already used for selection / explicit dirty).

## Impact

- `engine/src/runtime/function/render/transform_edit_viewport_notify.{h,cpp}` — new helpers
- `engine/src/tests/transform_edit_viewport_notify_test.cpp` — TDD coverage for generation bumps
- `engine/src/runtime/function/render/gizmo/transform_gizmo_controller.cpp` — wire `markSceneDirty`
- `engine/src/runtime/function/slint/slint_system.cpp` — wire `applyInspectorTransform`
- `docs/agents/render-pipeline.md` — document redraw-on-transform-edit
- No change to `tickVulkan` skip heuristics, pacing tiers, undo/redo, or multi-select transform
