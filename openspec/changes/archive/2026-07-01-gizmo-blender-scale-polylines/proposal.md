## Why

Transform gizmo handles appear oversized in small perspective viewports and inconsistent when toggling Persp/Iso. Root cause: our `computeView3dPixelSizeNoUiScale` diverges from Blender's `wm_gizmo_calculate_scale` + `ED_view3d_pixel_size` model (ortho uses `pixsize` only; perspective uses `pixsize × depth`). Rotation dials also use 3D tube geometry whose thickness scales with world `group_scale`, while Blender uses screen-space polylines (`GPU_SHADER_3D_POLYLINE`) at fixed pixel line width — making rings look chunky especially in perspective.

## What Changes

- **Blender-accurate gizmo scale:**
  - Introduce viewport `pixsize` derived from projection (Blender `rv3d->pixsize` / `BKE_camera_params_compute_viewplane` equivalent).
  - `group_scale = scale_basis × ui_scale × gizmo_size × pixel_size_no_ui_scale(pivot)`.
  - Orthographic: `pixel_size = pixsize` (depth-independent).
  - Perspective: `pixel_size = pixsize × depth` (depth = pivot view-space distance / `clip.w`).
  - Align ortho `pixsize` with `ortho_size / viewfac`; remove the current ortho/persp formula fork that causes ~`1/tan(fov/2)` mismatch on Numpad5 toggle.

- **Screen-space polyline dials:**
  - Replace 3D tube geometry for `dial`, `dial_wire`, and annulus outer ring wire with screen-space polyline rendering at Blender line widths (axis dials 3.0px, outer/annulus 2.0px / 1.0px planes).
  - Keep filled styles (`dial_fill` trackball, `dial_ghost` arc, `annulus` inner ring if filled) on existing geometry path.

- **Line width tuning:**
  - Match Blender `GIZMO_AXIS_LINE_WIDTH + 1.0f` for X/Y/Z rotation dials; `GIZMO_AXIS_LINE_WIDTH` for outer ring.
  - Reduce or remove tube-radius inflation now handled by pixel line width.

- **Small viewport cap (secondary):**
  - Optional effective `gizmo_size` clamp vs `min(viewport_w, viewport_h) × 0.35` (same pattern as navigate gizmo) so fixed 75px target does not dominate tiny docked viewports.

Out of scope: user preference slider for `U.gizmo_size`, HiDPI `U.pixelsize` beyond existing `ui_scale`, translate arrow / scale box polyline conversion.

## Capabilities

### New Capabilities

_(none)_

### Modified Capabilities

- `transform-gizmo`: Gizmo world scale follows Blender `wm_gizmo_calculate_scale`; rotation rings and outer wire rings render as screen-space polylines with Blender line widths; Persp/Iso toggle preserves similar screen size.

## Impact

- `engine/src/runtime/function/render/gizmo/gizmo_math.cpp` — `pixsize`, depth, `computeGizmoGroupScale`
- `engine/src/runtime/function/render/gizmo/gizmo_math.h` — extend `TransformGizmoScaleContext` (viewport width, projection rows)
- `engine/shaders/transform_gizmo.slang` — polyline vertex expansion + fragment; deprecate tube path for wire dials
- `engine/src/runtime/function/render/gizmo/transform_gizmo_overlay.cpp` — pass line width in pixels to shader
- `engine/src/runtime/function/render/gizmo/transform_gizmo_pick.cpp` — pick radius tracks screen line width + dial major radius
- `engine/src/runtime/function/render/overlay/grid_overlay.cpp` — optional shared `pixsize` helper for consistency
