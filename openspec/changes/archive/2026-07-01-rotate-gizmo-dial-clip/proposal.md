## Why

Rotate gizmo dials currently render as full 360° rings, which looks cluttered and unlike Blender's transform gizmo. Blender clips each dial to a camera-facing semicircle using a view-aligned GPU clip plane, and only shows the full ring while the user is actively dragging. Matching this behavior improves readability and editor polish.

## What Changes

- **Half-arc dial rendering (idle):** When rotate mode is active and the user is not dragging, each X/Y/Z dial is clipped to the half facing the camera via a world-space clip plane (Blender `ED_GIZMO_DIAL_DRAW_FLAG_CLIP`).
- **Full ring during drag (modal):** While a rotation drag is in progress, disable clip plane clipping so the active interaction shows a complete circle (Blender `!is_modal && CLIP`).
- **Clip plane construction:** Plane normal = camera-to-pivot view direction; plane passes through gizmo pivot with a small pixel-scaled bias to reduce edge flicker (Blender `DIAL_CLIP_BIAS`).
- **Shader clip pass:** Extend `transform_gizmo.slang` fragment stage to discard fragments behind the clip plane when enabled; pass plane + enable flag from overlay uniforms.
- **Ghost arc unchanged:** `dial_ghost` drag feedback keeps existing angular arc clipping; half-arc clip is not applied to the ghost.

Out of scope: rotation angle numeric overlay (`WM_GIZMO_DRAW_VALUE`), trackball outer ring, pick behavior changes beyond optional clip-plane consistency.

## Capabilities

### New Capabilities

_(none)_

### Modified Capabilities

- `transform-gizmo`: Rotate dials MUST render as camera-facing semicircles when idle and full circles during active rotation drag.

## Impact

- `engine/shaders/transform_gizmo.slang` — clip plane uniform, fragment discard
- `engine/src/runtime/function/render/gizmo/transform_gizmo_shared.h` — uniform layout extension
- `engine/src/runtime/function/render/gizmo/transform_gizmo_overlay.cpp` — clip plane build, enable/disable on drag
- `engine/src/runtime/function/render/gizmo/gizmo_math.h` / `gizmo_math.cpp` — optional `buildDialClipPlane` helper
- `engine/src/runtime/function/render/gizmo/transform_gizmo_types.h` — `k_dial_clip_bias` constant
