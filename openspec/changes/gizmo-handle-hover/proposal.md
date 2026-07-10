## Why

Transform gizmo handles only highlight while dragging (`color_hi` on the active axis). Blender shows hover feedback (`WM_GIZMO_DRAW_HOVER`) before click, which helps users see which handle they will grab. This was explicitly deferred in `gizmo-rotate-trackball-scale-style`; the overlay and pick infrastructure are now stable enough to add hover without new shaders.

## What Changes

- Track hovered `ManipulatorAxis` on `TransformGizmoController` via CPU analytic pick on viewport `MouseMoved` (when not dragging).
- Reuse existing `color_hi` highlight path in `TransformGizmoOverlay` for hovered handles (same visual as active drag).
- Add `pickTransformGizmoHandle(mode, ctx)` wrapper and `gizmoHandleHighlighted(active, hover, axis)` helper.
- Wire `RenderSystem::onEvent` to update hover and `requestViewportRedraw()` only when hovered axis changes; do **not** set `event.handled` (mesh pick and camera input unchanged).
- Unit test for highlight helper and pick wrapper.
- Document hover path in `docs/agents/render-pipeline.md`.

Out of scope: cursor shape changes, hover on non-interactive decor (trackball `rot_t`, outer ring `rot_c`, scale annulus `scale_c_outer`), navigate gizmo hover.

## Capabilities

### New Capabilities

_(none)_

### Modified Capabilities

- `transform-gizmo`: Add hover highlight requirement for interactive handles in translate/rotate/scale modes; drag highlight takes precedence over hover.

## Impact

- `engine/src/runtime/function/render/gizmo/transform_gizmo_pick.{h,cpp}`
- `engine/src/runtime/function/render/gizmo/transform_gizmo_controller.{h,cpp}`
- `engine/src/runtime/function/render/gizmo/transform_gizmo_overlay.cpp`
- `engine/src/runtime/function/render/render_system.cpp`
- `engine/src/tests/transform_gizmo_hover_test.cpp`, `engine/src/tests/CMakeLists.txt`
- `docs/agents/render-pipeline.md`
