## Why

After `fix-gizmo-dial-render`, transform gizmo geometry is visible but looks chunky and unlike Blender: thick cross-billboard stems without cone tips, oversized plane squares, square center handle instead of a white disc, thick rotation torus bands, and scale mode drawing flat quads instead of axis-end cubes. The visibility-first thicken pass solved rendering bugs but left poor editor polish. This change refines silhouettes and styling to match Blender's transform gizmo while preserving existing pick/drag behavior.

## What Changes

- **Archive `fix-gizmo-dial-render`:** Remaining manual validation and aesthetic tuning move here; the dial degenerate-triangle root cause is already fixed in code.
- **Move gizmo — Blender silhouette:**
  - Axis arrows: thin stem + cone tip (not thick cross-billboard blocks).
  - Plane handles: smaller squares at axis corners (Blender `ED_GIZMO_ARROW_STYLE_PLANE` proportions).
  - Center handle: white/gray view-aligned disc (`GizmoDrawStyle::center`), not a square quad.
- **Rotate gizmo — thin rings:** Reduce dial tube thickness to Blender-like proportions while keeping dials visible after the prior shader fix.
- **Scale gizmo — axis-end cubes:** Wire overlay to `GizmoDrawStyle::scale_box` for per-axis tips and center uniform cube.
- **Visual polish:** Flat UI-style shading (minimal fake lighting), tune alpha and metrics in `TransformGizmoMetrics` to match Blender defaults.
- **Pick tolerances:** Adjust pick radii/thresholds to match new geometry sizes (behavior unchanged).

Out of scope: screen-space constant-width lines, rotate trackball outer ring, hover highlight, gizmo size preferences UI.

## Capabilities

### New Capabilities

_(none)_

### Modified Capabilities

- `transform-gizmo`: Extend visual requirements so translate, rotate, and scale gizmos match Blender-style silhouettes (cone arrows, small plane handles, center disc, thin dials, axis-end scale cubes) without regressing visibility or interaction.

## Impact

- `engine/shaders/transform_gizmo.slang` — `vsArrow` cone tip, thinner `vsDial` tube, flat fragment shading
- `engine/src/runtime/function/render/gizmo/transform_gizmo_overlay.cpp` — center/scale style wiring, layout constants
- `engine/src/runtime/function/render/gizmo/transform_gizmo_types.h` — metric tuning (stem radius, plane extent, dial tube)
- `engine/src/runtime/function/render/gizmo/transform_gizmo_pick.cpp` — pick thresholds for new geometry sizes
- `openspec/changes/fix-gizmo-dial-render/` — archived (superseded for remaining work)
