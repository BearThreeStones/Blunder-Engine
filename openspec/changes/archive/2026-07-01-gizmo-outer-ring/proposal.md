## Why

Rotate and scale gizmos are missing Blender's view-aligned outer white rings: rotate mode has only three colored semicircular axis dials (no trackball outer circle), and scale mode has axis cubes plus a center cube but no dual-concentric annulus. These rings are key visual anchors for view-relative manipulation and complete the Blender transform gizmo silhouette.

## What Changes

- **Rotate outer ring (`MAN_AXIS_ROT_C` equivalent):**
  - View-aligned white/gray ring using `TH_GIZMO_VIEW_ALIGN` color
  - Full 360° wire circle (no clip plane — Blender `ED_GIZMO_DIAL_DRAW_FLAG_NOP`)
  - Scale basis 1.2× relative to axis dials (larger outer trackball circle)
  - Drawn behind or beneath colored X/Y/Z semicircular dials

- **Scale outer annulus (`MAN_AXIS_SCALE_C` equivalent):**
  - View-aligned white/gray dual wire circles (Blender `ED_GIZMO_PRIMITIVE_STYLE_ANNULUS`)
  - Outer radius 1.0, inner radius `1.0 / arc_inner_factor` (factor 6.0) in local mesh space
  - Gizmo scale basis ~0.2× for the annulus handle (smaller than axis cubes)
  - Drawn around the existing center uniform scale cube

- **New draw styles & axes:**
  - `GizmoDrawStyle::dial_wire` (or dial variant with NOP / no clip / thin tube) for rotate outer ring
  - `GizmoDrawStyle::annulus` for scale outer dual circles
  - New `ManipulatorAxis::rot_c` and `ManipulatorAxis::scale_c_outer` (names per design)

- **Color:** Reuse existing `kGizmoViewAlignColor` / `centerColor()` for both outer rings

Out of scope (follow-up): trackball rotation drag on `rot_c`, annulus uniform-scale drag, numeric angle overlay (`WM_GIZMO_DRAW_VALUE`), pick on outer rings unless trivial wire hit-test is included in MVP.

## Capabilities

### New Capabilities

_(none)_

### Modified Capabilities

- `transform-gizmo`: Rotate mode MUST show a view-aligned white outer ring; scale mode MUST show a view-aligned white annulus (dual concentric wire circles).

## Impact

- `engine/shaders/transform_gizmo.slang` — `dial_wire`, `annulus` procedural geometry
- `engine/src/runtime/function/render/gizmo/transform_gizmo_types.h` — new axes, metrics, draw styles
- `engine/src/runtime/function/render/gizmo/transform_gizmo_overlay.cpp` — draw paths for rotate/scale outer rings
- `engine/src/runtime/function/render/gizmo/transform_gizmo_shared.h` — vertex counts, UBO if needed
- `engine/src/runtime/function/render/gizmo/gizmo_math.cpp` — view-aligned matrices, scale basis
- Builds on `rotate-gizmo-dial-clip` (axis dials clipped; outer ring explicitly unclipped)
