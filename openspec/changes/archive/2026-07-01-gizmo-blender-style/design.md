## Context

Transform gizmo rendering lives in `TransformGizmoOverlay` + `transform_gizmo.slang`, with metrics and colors in `TransformGizmoMetrics` / `transform_gizmo_types.cpp`. Prior changes (`fix-transform-gizmo`, `fix-gizmo-dial-render`) restored visibility: mode persistence, scale interaction, dial degenerate-triangle fix, and thickened non-plane geometry. User feedback shows gizmos are functional but visually chunky—not Blender-like.

Current mismatches:

| Element | Current | Target (Blender) |
|---------|---------|------------------|
| Arrow | 3 cross-billboard stem, no tip | Thin stem + cone tip |
| Plane handle | Large quad (`0.1` half-extent) | Small corner square (~0.1 Blender local) |
| Center | Square plane quad | White/gray view-aligned disc |
| Dial | Thick torus (`tubeW ≈ 0.10`) | Thin ring |
| Scale | `GizmoDrawStyle::plane` quads | `GizmoDrawStyle::scale_box` cubes |
| Shading | Lambert fake lighting | Flat UI color |

`GizmoDrawStyle::center` and `scale_box` exist in shader but overlay routes center/scale through `plane`.

## Goals / Non-Goals

**Goals:**

- Move: cone-tipped arrows, small plane handles, white/gray center disc.
- Rotate: thin visible rings (post dial shader fix).
- Scale: axis-end cubes + center uniform cube via `scale_box`.
- Tune `TransformGizmoMetrics` to Blender reference values already documented in headers.
- Update pick thresholds to match new geometry without changing drag math.
- Flat fragment shading for crisp UI-widget appearance.

**Non-Goals:**

- Screen-space constant-width lines (future pass).
- Rotate trackball outer ring (view-relative rotation handle).
- Hover highlight (only drag highlight exists today).
- Gizmo size preferences UI.
- Changes to global/local space, snapping, or multi-selection.

## Decisions

### 1. Extend `vsArrow` with cone tip geometry

**Decision:** Add a cone tip segment at the positive Z end of the arrow local frame (stem along +Z), keeping the existing stem as a single billboard quad or thin cross pair. Total vertex budget stays within one draw call; target ≤ 36 vertices (stem + 8–12 sided cone).

**Alternatives:**

- Full Blender mesh import: overkill; procedural is sufficient.
- Line-list overlay pass: deferred (non-goal).

**Rationale:** Cone tip is the largest visual gap vs Blender; stem-only cross billboards read as thick blocks.

### 2. Wire overlay to existing `center` and `scale_box` styles

**Decision:**

- `trans_c` in translate mode → `GizmoDrawStyle::center` with `gizmoViewAlignedCenterMatrix` (already used for matrix).
- Scale mode → `GizmoDrawStyle::scale_box` instead of `plane`; remove `k_scale_tip_quad_layout` plane workaround.

**Rationale:** Shader paths already exist; overlay wiring is the missing link.

### 3. Reduce thickness after visibility is confirmed

**Decision:** Lower dial `tubeW`, arrow `stemHalfW`, and remove aggressive `lineWidthScale()` minimum floors added in `fix-gizmo-dial-render`. Keep a small floor (e.g. 0.5×) to prevent sub-pixel disappearance on low-DPI viewports.

**Metrics targets (local space, pre group scale):**

| Constant | Current | Target |
|----------|---------|--------|
| `k_mesh_stem_radius` | 0.09 | ~0.04–0.05 |
| `k_mesh_plane_half_extent` | 0.1 | 0.08–0.1 (keep offset 0.2) |
| `k_mesh_dial_tube_radius` | 0.08 | ~0.03–0.04 |
| `k_mesh_center_radius` | 0.22 | ~0.18–0.22 |

Tune during validation; shader and header constants must stay in sync.

### 4. Flat UI fragment shading

**Decision:** Replace or reduce Lambert `ndl` in `fragmentMain` to near-flat color: `return float4(input.color.rgb, input.color.a)` or minimal contrast (0.9–1.0 range).

**Rationale:** Blender gizmos read as flat UI overlays, not lit scene geometry.

### 5. Pick tolerance follow geometry

**Decision:** Adjust `pickThreshold`, plane half-extent, and dial tube pick margin in `transform_gizmo_pick.cpp` to match reduced handle sizes. No change to drag/controller logic.

### 6. Archive `fix-gizmo-dial-render`

**Decision:** Move `openspec/changes/fix-gizmo-dial-render/` to `openspec/changes/archive/2026-06-17-fix-gizmo-dial-render/` when this change is proposed. Remaining tasks 4.1/4.2 manual validation become part of `gizmo-blender-style` validation.

## Risks / Trade-offs

- **[Risk] Thinner geometry becomes invisible again** → Keep small `lineWidthScale` floor; validate at 1080p and 4K before merging.
- **[Risk] Cone tip increases vertex count / draw budget** → Stay within `k_max_gizmo_draws_per_frame` (12); arrow still one draw per axis.
- **[Risk] Pick misses after size reduction** → Tune `k_pick_slop`; manual click-test each handle type.
- **[Risk] Metrics drift between shader literals and C++ headers** → Single source: update both `transform_gizmo.slang` and `TransformGizmoMetrics` together.

## Migration Plan

Single PR after `fix-gizmo-dial-render` archive. No data migration.

Manual validation:

1. G → cone arrows + small planes + white center disc; drag each handle.
2. R → three thin rings; drag rotation; ghost arc on drag.
3. S → axis-end cubes + center cube; axis and uniform scale drag.
4. Switch selection → mode preserved; gizmo repositioned.
5. Compare side-by-side with Blender default transform gizmo at similar zoom.

Rollback: revert commits; no persistent state.

## Open Questions

- Exact cone segment count (8 vs 12 sides)? **Default:** 8 sides for performance.
- Center disc alpha: fully opaque or 0.85 like axes? **Default:** opaque white/gray (`k_gizmo_color_alpha_hi` for center only).
