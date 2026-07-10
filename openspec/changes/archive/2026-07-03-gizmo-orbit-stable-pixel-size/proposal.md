## Why

After `gizmo-blender-scale-polylines`, transform gizmo **overall size** (arrows, rings, center — uniform `group_scale`) drifts when the user right-click orbits the perspective camera, even with the selection pivot at the viewport center. Root cause: `computeViewportPixsize()` derives `pixsize` from **full view×projection matrix columns**, which change with yaw/pitch, while pivot depth (`clip.w`) stays constant on the view axis — so `pixel_size = pixsize × depth` is not invariant during orbit. The pre-change formula used a view-independent `(2/viewport_height)` factor and was orbit-stable at the focal point.

## What Changes

- **Split pixsize from depth source:**
  - `pixsize` from **projection matrix only** (Blender `winmat` equivalent), not `view × projection`.
  - Pivot depth (`zfac`) still from full `view_projection × pivot` (Blender `mul_project_m4_v3_zfac` / `clip.w`).
- **Orbit invariance requirement:** When the gizmo pivot projects to the viewport center and orbit distance is fixed, `group_scale` MUST remain approximately constant (within 5%).
- **Preserve `gizmo-blender-scale-polylines` wins:** Persp/Iso toggle parity, small-viewport cap, polyline dial line widths unchanged.
- **Extend scale context:** Pass separate `projection` and `view_projection` (or projection-only path) into `TransformGizmoScaleContext` from overlay and controller.

Out of scope: orbit-around-selection focal, viewport click-to-select, changing polyline shaders or dial clip behavior.

## Capabilities

### New Capabilities

_(none)_

### Modified Capabilities

- `transform-gizmo`: Add orbit-stable perspective pixel-size behavior; clarify pixsize vs depth split in gizmo scale requirement.

## Impact

- `engine/src/runtime/function/render/gizmo/gizmo_math.h` / `gizmo_math.cpp` — `computeViewportPixsize`, `TransformGizmoScaleContext`
- `engine/src/runtime/function/render/gizmo/transform_gizmo_overlay.cpp` — pass projection separately
- `engine/src/runtime/function/render/gizmo/transform_gizmo_controller.cpp` — pick/drag scale context
- Optional: debug assert or unit-style check for orbit `group_scale` variance
