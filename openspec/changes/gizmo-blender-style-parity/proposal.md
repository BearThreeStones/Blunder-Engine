## Why

Transform and navigate gizmos used approximate theme colors and navigate-specific flat alphas. Blender 4.x uses default theme axis RGB, depth-based navigate fading, and hover backdrop — users expect visual parity with Blender.

## What Changes

- Align `TH_AXIS_*` and `TH_GIZMO_VIEW_ALIGN` with Blender default theme (`userdef_default_theme.c`).
- Add `navigate_gizmo_style` helpers mirroring `VIEW3D_GT_navigate_rotate` color math.
- Navigate gizmo diameter 80px, `AXIS_HANDLE_SIZE` 0.20, depth fade in shader.
- Navigate hover: `color_hi` backdrop + per-axis label highlight on `MouseMoved`.
- Unit tests: `navigate_gizmo_style_test.cpp`, extended `transform_gizmo_hover_test.cpp`.

## Capabilities

### Modified Capabilities

- `transform-gizmo`: Theme RGB parity with Blender default.
- `navigate-gizmo`: Blender visual style and hover (new capability spec).

## Impact

- `coordinate_system.h`, `navigate_gizmo_*`, `navigate_gizmo.slang`, `render_system.cpp`
- `engine/src/tests/navigate_gizmo_style_test.cpp`
