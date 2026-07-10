## Context

After Batch 1, translate center disc and arrow stems use local 2D SDF quads with Jacobian AA (`kStrokeSdfDisc`, `kStrokeSdfSegment`). Plane handles (`GizmoDrawStyle::plane`, style `1`) still render as solid Z-facing quads via `vsPlane` → `emitV` (opaque fill, aliased edges). Scale axis handles (`scale_stem_box`, style `9`) render the stem as a solid XZ quad and the cube via `vsScaleBox`.

The shader still contains unused screen-space polyline infrastructure from the pre-Batch-1 arrow stem (`emitPolyline`, `polylineExtrude`, fragment `smoothline` for `strokeCoord ∈ [-1,1]`). No draw call emits that stroke range anymore.

**Constraint (from Batch 1):** All new AA MUST use local 3D-anchored SDF quads — no screen-pixel or NDC-circle fragment shaders (caused white-ring camera drift).

Plane layout is already passed per-draw via `ubo.quad_layout` (`cx, cy, hx, hy`) from `transform_gizmo_overlay.cpp` (`k_plane_quad_layout`, `k_scale_plane_quad_layout`).

## Goals / Non-Goals

**Goals:**

- Plane handles (translate + scale): crisp rectangle **border** strokes via SDF, pivot-locked, fixed pixel width.
- Scale stem: same segment SDF path as translate arrow stem along local +Z.
- CPU TDD mirrors for rectangle border alpha (`sdBox2d`, `rectRingAlpha`).
- Remove dead polyline vertex/fragment code from `transform_gizmo.slang`.
- All existing gizmo tests pass; pick/hover unchanged.

**Non-Goals:**

- Scale cube / center cube mesh changes (remain solid `emitV`).
- Changing `TransformGizmoUniformData` layout or adding varyings.
- Updating stale `outline-gizmo-anti-aliasing` rotation-dial polyline spec (separate change).
- Screen-pixel ring rendering.

## Decisions

### 1. Rectangle border via box SDF stroke (`kStrokeSdfRect = -5.0`)

**Choice:** `sdBox2d(p, half_extents)` for signed distance to axis-aligned rectangle; `rectRingAlpha` applies the same Jacobian + `fwidth` pattern as `ringLineAlpha` but on `abs(sd) - halfWLocal` (stroke centered on the box edge). Vertex: `vsSdfRectQuad` pads bbox from `ubo.quad_layout` like `vsSdfRingQuad` pads a circle.

**Alternatives:** Four separate `vsSdfSegmentQuad` edges (4× draw complexity in one quad); filled rect with `discFillAlpha`-style (wrong visual — Blender planes are bordered squares).

**Rationale:** Single 6-vert quad per plane handle; reuses established SDF AA family; `quad_layout` already encodes center and half-extents.

### 2. Fragment marker ordering

**Choice:** Extend thresholds: `< -4.5` rect, `< -3.5` segment, `< -2.5` disc, `< -1.5` ring. `kStrokeSdfRect = -5.0`.

**Rationale:** Keeps Batch 1 markers intact; monotonic ordering avoids branch ambiguity.

### 3. Scale stem segment endpoints in fragment

**Choice:** When `strokeCoord < -3.5`, branch on `style`: arrow (`0`) uses `(0,0)→(0, stemLen)`; `scale_stem_box` (`9`) uses `(0, kMeshScaleStemStart)→(0, kMeshScaleBoxOffset)`.

**Alternatives:** New varyings (layout change); pack endpoints in `ringMajorR` (opaque).

**Rationale:** Same pattern as Batch 1 arrow hardcoding; only two segment draw styles exist.

### 4. CPU helpers before shader (TDD)

**Choice:** Add `sdBox2d`, `rectRingAlpha` to `gizmo_sdf_aa` with tests before shader edits.

**Rationale:** Consistent with Batch 1; rectangle SDF math is non-trivial to eyeball.

### 5. Polyline dead-code removal after migrations

**Choice:** Delete `emitPolyline`, `polylineExtrude`, and smoothline fragment branch only after plane + scale stem land and tests pass.

**Rationale:** Avoid breaking intermediate builds; `gizmo_polyline_aa` remains for any future non-gizmo polylines.

## Risks / Trade-offs

| Risk | Mitigation |
|------|------------|
| Plane border too thin/thick vs solid fill | Match `line_width_px` + `kPolylineSmoothWidth` half-width; visual QA translate + scale modes |
| Rect SDF corner joins look pinched | Box SDF gives correct mitered corners; compare to Blender reference |
| Scale stem width mismatch vs arrow | Reuse same `vsSdfSegmentQuad` / `segmentLineAlpha`; shared `lineWidthScale()` |
| Removing polyline path breaks unknown caller | Grep confirms `emitPolyline` uncalled; run full gizmo test suite |
| Filled plane → bordered plane visual change | Intentional Blender parity; semi-transparent fill if needed later (out of scope) |

## Migration Plan

1. Land CPU `sdBox2d` + `rectRingAlpha` + tests (no visual change).
2. Switch plane style to `vsSdfRectQuad` + fragment rect path.
3. Switch scale stem to `vsSdfSegmentQuad`; extend fragment segment branch for style `9`.
4. Remove polyline dead code.
5. Run gizmo test suite + manual QA (translate planes, scale stems/boxes, rotate rings unchanged).

Rollback: revert shader + `gizmo_sdf_aa` additions; no data migration.

## Open Questions

- None blocking. If plane handles should retain partial fill inside the border, defer to a follow-up (current spec: bordered corner squares).
