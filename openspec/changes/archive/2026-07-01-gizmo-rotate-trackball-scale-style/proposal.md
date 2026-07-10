## Why

Rotate and scale gizmo handle styling is still incomplete versus Blender after `gizmo-blender-style`, `rotate-gizmo-dial-clip`, and `gizmo-outer-ring`. Rotate mode lacks the trackball overlay (`MAN_AXIS_ROT_T` — filled view-aligned disc at ~5% alpha). Scale mode uses axis-end cubes instead of Blender's box+stem arrow handles and has no XY/YZ/ZX plane handles. Aligning these remaining handle styles completes Blender parity for transform manipulation visuals.

## What Changes

- **Rotate trackball overlay (`MAN_AXIS_ROT_T`):**
  - View-aligned filled dial (`ED_GIZMO_DIAL_DRAW_FLAG_FILL`) at pivot
  - Very low alpha (~0.05 × theme alpha) — subtle background disc
  - Drawn beneath axis dials and outer ring; no semicircle clip
  - Pick/drag deferred (visual only in MVP)

- **Rotate axis dial polish (verify/tune against Blender codemap):**
  - X/Y/Z dials: `CLIP` when idle, line width = `GIZMO_AXIS_LINE_WIDTH + 1` (3.0)
  - Axis colors: TH_AXIS_X/Y/Z (already themed)
  - Rotation rings never view-fade (alpha_fac = 1.0) — already behavior for `rot_x/y/z`
  - Outer ring `rot_c` and trackball layering order matches Blender draw order

- **Scale axis handles — box+stem style:**
  - Replace or restyle axis-end cubes as Blender `ED_GIZMO_ARROW_STYLE_BOX` + stem
  - Stem from pivot offset (~0.2 local) to box at axis end (~0.8 local)
  - Per-axis RGB colors (reuse `gizmoAxisColor`)

- **Scale plane handles (XY, YZ, ZX):**
  - Add small plane squares at axis corners (`ED_GIZMO_ARROW_STYLE_PLANE`, length ~0.7×)
  - Two-axis scale constraint via existing scale drag masks

Out of scope: `WM_GIZMO_DRAW_VALUE` numeric HUD, trackball drag interaction (`TRANSFORM_OT_trackball`), hover highlight states, scale plane handles when translate gizmo also visible.

## Capabilities

### New Capabilities

_(none)_

### Modified Capabilities

- `transform-gizmo`: Rotate mode adds trackball overlay; scale mode uses box+stem axis handles and plane corner handles matching Blender silhouettes.

## Impact

- `engine/shaders/transform_gizmo.slang` — `dial_fill` or extend `dial` with fill flag; `scale_box_stem` or extend arrow/box geometry
- `engine/src/runtime/function/render/gizmo/transform_gizmo_types.h` — `rot_t`, scale plane axes, metrics, draw styles
- `engine/src/runtime/function/render/gizmo/transform_gizmo_overlay.cpp` — draw order, scale handle paths
- `engine/src/runtime/function/render/gizmo/transform_gizmo_pick.cpp` — scale plane pick (optional stretch)
- Builds on completed `gizmo-outer-ring`, `rotate-gizmo-dial-clip`
