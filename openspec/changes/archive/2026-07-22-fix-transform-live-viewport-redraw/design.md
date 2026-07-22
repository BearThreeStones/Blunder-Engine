## Context

Entity TRS and `syncSceneToRender` already refresh every frame. The viewport still looks stale because `RenderSystem::tickVulkan` skips the offscreen pass when camera matrices are unchanged and `m_viewport_render_generation` / `m_force_viewport_render` are idle.

Selection present and gizmo mode/space changes already call `requestViewportRedraw()`. Transform gizmo drag updates the entity via an anonymous `markSceneDirty()` helper but never requests a redraw. Inspector TRS apply (`SlintSystem::applyInspectorTransform`) updates the entity without `markViewportDirtyRegion` or `requestViewportRedraw`.

Confirmed symptoms:

1. Inspector numbers update live during gizmo drag (`syncInspectorLive` works).
2. Mesh and gizmo handles stay frozen until the camera moves (`camera_changed` opens the skip gate).

## Goals / Non-Goals

**Goals:**

- Force a viewport redraw on gizmo transform dirty (drag, release, Escape cancel) so mesh and handles track under a static camera.
- Force Slint dirty + viewport redraw when Inspector position/rotation/scale fields apply.
- Centralize both paths behind testable notify helpers.

**Non-Goals:**

- Changing `tickVulkan` skip heuristics or interactive/idle pacing tiers.
- Draw-list caching, undo/redo, or multi-select transform.
- Broader Inspector dirty-region policy beyond transform apply.

## Decisions

### 1. Extend `editor-viewport-pacing`, not `transform-gizmo`

**Decision:** Spec delta lives on `editor-viewport-pacing` because that capability already owns the `requestViewportRedraw()` / dirty-generation contract (selection change and explicit redraw bypass idle throttling).

**Alternatives considered:**

- **`transform-gizmo`:** Covers manipulator visuals and drag math, not the frame-skip / redraw gate. Rejected for this requirement.

### 2. Thin notify helpers + TDD

**Decision:** Introduce free functions in `transform_edit_viewport_notify.{h,cpp}`:

- `notifyViewportAfterGizmoTransformEdit(RenderSystem*)` → `requestViewportRedraw()`
- `notifyViewportAfterInspectorTransformEdit(RenderSystem*, SlintSystem*)` → `markViewportDirtyRegion()` (if Slint non-null) + `requestViewportRedraw()`

Null pointers are no-ops for the missing system so unit tests can exercise generation bumps without a full Slint host.

**TDD artifacts:**

- `engine/src/runtime/function/render/transform_edit_viewport_notify.{h,cpp}`
- `engine/src/tests/transform_edit_viewport_notify_test.cpp`

**Alternatives considered:**

- Inline `requestViewportRedraw` at each call site — easy to miss Escape cancel / release paths.
- Always require a live `SlintSystem` in tests — heavier than needed for the Vulkan generation contract.

### 3. Single gizmo chokepoint: `markSceneDirty`

**Decision:** Call `notifyViewportAfterGizmoTransformEdit` only from the existing anonymous `markSceneDirty()` in `transform_gizmo_controller.cpp`, so drag, release, and Escape cancel cannot drift apart.

### 4. Mirror selection present for Inspector

**Decision:** In `applyInspectorTransform`, call `notifyViewportAfterInspectorTransformEdit` so Inspector edits match the selection present pattern (`markViewportDirtyRegion` + `requestViewportRedraw`).

## Risks / Trade-offs

- **[Risk] Extra redraws during continuous gizmo drag** → Acceptable; interactive pacing tier already expects gizmo drag to schedule composites. Mitigation: reuse existing `requestViewportRedraw` (generation bump), do not bypass pacing floors.
- **[Risk] Helper forgotten on a future transform path** → Mitigation: document in `render-pipeline.md`; keep gizmo path funneled through `markSceneDirty`.
- **[Risk] Null Slint in production Inspector path** → Unlikely while UI is up; helper still forces Vulkan redraw via `RenderSystem`.

## Migration Plan

Single PR, no data migration. Validate:

1. Translate-gizmo drag with static camera — mesh and handles move each frame.
2. Inspector position edit with static camera — mesh and gizmo jump to new TRS.
3. Escape cancel during drag still restores TRS and redraws.
4. Unit test `transform_edit_viewport_notify_test` passes.

Rollback: revert commits; no persistent state.

## Open Questions

- _(none — behavior and call sites confirmed by explore / implementation plan)_
