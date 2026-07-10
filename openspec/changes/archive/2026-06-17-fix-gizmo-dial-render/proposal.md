## Why

After `fix-transform-gizmo`, mode persistence and scale interaction landed, but non-plane gizmo geometry still does not render reliably in the viewport. Rotate mode shows an empty scene (no rotation dials) while the toolbar correctly highlights Rotate; Move mode shows plane handles but not axis arrows; Scale mode likely suffers the same class of failure. Users cannot see or use rotate/scale manipulators, blocking basic editing.

## What Changes

- **Fix rotate dial rendering (primary):** Make `GizmoDrawStyle::dial` rings visible at the selection pivot in rotate mode—thicker tube, shader fixes, and/or pipeline state verification.
- **Fix remaining non-plane styles:** Restore visibility for `arrow` (translate axes) and `scale_box` (scale handles), which share the same overlay shader path as dials but fail while `plane` succeeds.
- **Viewport refresh on gizmo-only changes:** Ensure switching transform mode always produces a new offscreen frame with updated overlays (no stale Slint viewport texture).
- **Verification:** Manual and/or RenderDoc checks for dial draw calls (`vkCmdDraw` × 3, style=3) when Rotate is active with a selected entity.

Out of scope: new manipulator features, screen-space constant-width lines, gizmo size preferences UI.

## Capabilities

### New Capabilities

_(none)_

### Modified Capabilities

- `transform-gizmo`: Extend requirements so rotate dials, translate arrows, and scale boxes MUST render visibly—not only translate plane handles.

## Impact

- `engine/shaders/transform_gizmo.slang` — `vsDial`, `vsArrow`, `vsScaleBox`, `lineWidthScale`
- `engine/src/runtime/function/render/gizmo/transform_gizmo_overlay.cpp` — draw paths, uniform params
- `engine/src/runtime/function/render/vulkan/` — confirm depth/blend pipeline state for screen overlay pass
- `engine/src/runtime/function/render/render_system.cpp` — gizmo-only viewport invalidation if needed
- Builds on completed `fix-transform-gizmo` (mode persistence, scale pick/drag already in place)
