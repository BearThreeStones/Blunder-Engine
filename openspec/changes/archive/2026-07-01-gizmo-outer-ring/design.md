## Context

Current transform gizmo (`TransformGizmoOverlay` + `transform_gizmo.slang`):

| Mode | Drawn today | Missing (Blender) |
|------|-------------|-------------------|
| Rotate | 3 colored dials (semicircle clip via `rotate-gizmo-dial-clip`) | White view-aligned **outer full circle** (`MAN_AXIS_ROT_C`) |
| Scale | 3 axis cubes + center cube | White view-aligned **annulus** (`MAN_AXIS_SCALE_C`) |

Existing pieces to reuse:

- `gizmoViewAlignedCenterMatrix()` — view-aligned basis (translate center, scale center cube)
- `kGizmoViewAlignColor` / `centerColor()` — `TH_GIZMO_VIEW_ALIGN`
- `k_rotate_dial_scale_basis = 1.2f` — already matches Blender outer ring scale (currently applied to axis dials; outer ring should own this basis)
- `vsDial` ring geometry — can variant for thin wire + NOP (no clip)

Blender reference:

```
Rotate outer (rot_c):  dial, NOP, scale 1.2, white, full circle r=1.0
Scale outer (scale_c): annulus, inner=r/6, outer r=1.0, scale 0.2, white
```

## Goals / Non-Goals

**Goals:**

- Rotate: white view-aligned full wire ring, 1.2× scale basis, no clip
- Scale: white view-aligned dual wire circles (annulus), 0.2× scale basis
- Correct draw order and colors; stay within `k_max_gizmo_draws_per_frame` (12)

**Non-Goals:**

- Trackball rotation drag on outer ring (view-relative rotation math)
- Scale drag on annulus (uniform scale from outer ring)
- Pick hit-test on outer rings (optional stretch; default defer)
- Numeric rotation value HUD
- Screen-space constant-width lines

## Decisions

### 1. New manipulator axes

**Decision:** Add to `ManipulatorAxis`:

- `rot_c` — rotate view/trackball outer ring
- `scale_c_outer` — scale view annulus (distinct from `trans_c` center cube)

**Rationale:** Blender treats these as separate gizmo widgets with different draw styles and scale bases.

### 2. New draw styles

**Decision:**

| Style | Use | Key params |
|-------|-----|------------|
| `dial_wire` (6) | Rotate outer ring | thin tube, majorR=1.0, clip_enabled=0 always |
| `annulus` (7) | Scale outer ring | outerR=1.0, innerR=1/6, thin wire |

Alternative: overload `dial` with uniform flag — rejected; explicit styles simplify overlay and shader branching.

### 3. Rotate outer ring geometry

**Decision:** Reuse dial quad-strip path with:

- `major_radius = 1.0` (vs axis dials `0.82`)
- Thinner `tubeW` (wireframe look, ~half axis dial tube)
- `clip_enabled = 0` always (Blender NOP)
- Matrix: `gizmoViewAlignedCenterMatrix(basis, handle_scale, camera)`
- `handle_scale = group_scale * k_rotate_outer_ring_scale_basis` (1.2f)
- Color: `centerColor()`, alpha 1.0

Draw order: outer ring **first**, then axis dials on top.

### 4. Scale annulus geometry

**Decision:** New `vsAnnulus` in shader:

- Two dial-like ring passes in one draw OR one vertex stream with inner/outer segments
- Outer circle: radius 1.0; inner: `1.0 / k_arc_inner_factor` (6.0 → ~0.167)
- Thin wire tube for both
- Matrix: view-aligned center matrix
- `handle_scale = group_scale * k_scale_outer_ring_scale_basis` (0.2f)
- Color: `centerColor()`

Blender draws inner at `arc_inner_factor` in some code paths but ANNULUS semantics use **inner = outer / factor**; match visual (small inner, large outer).

### 5. Scale basis reassignment

**Decision:** Axis rotation dials use scale basis **1.0**; outer ring uses **1.2**. Today `k_rotate_dial_scale_basis = 1.2` applies to `rot_x/y/z` — change axis dials to 1.0 and assign 1.2 to `rot_c` only.

**Impact:** Slight size reduction of colored dials; matches Blender (axis dials smaller than outer trackball).

### 6. Draw budget

**Decision:** Increase `k_max_gizmo_draws_per_frame` from 12 → 14 if needed (rotate +1 outer, scale +1 annulus). Current worst case: translate 7, rotate 3+1+1 ghost=5, scale 4+1=5 — fits in 12. No increase required.

## Risks / Trade-offs

- **[Risk] Axis dial size change when moving scale_basis** → Expected Blender parity; verify in manual QA
- **[Risk] Annulus inner/outer radius interpretation** → Tune to match Blender screenshot; document constants in `TransformGizmoMetrics`
- **[Risk] UBO layout drift** → Reuse existing clip fields; new styles default `clip_enabled=0`; keep `static_assert`

## Migration Plan

Single PR after `rotate-gizmo-dial-clip` layout fix lands. Manual validation:

1. Rotate → white full outer ring + three colored semicircular dials
2. Scale → white dual circles + axis cubes + center cube
3. Orbit camera → outer rings stay view-aligned

## Open Questions

- Pick on outer rings in MVP? **Default:** defer; drawing-only first
- Trackball drag on `rot_c`? **Default:** defer to follow-up change
