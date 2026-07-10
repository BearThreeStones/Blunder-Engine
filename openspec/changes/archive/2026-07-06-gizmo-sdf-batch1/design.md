## Context

Rotate gizmo rings (`dial`, `dial_wire`, `annulus`) render via `vsSdfRingQuad` + `ringLineAlpha` in `transform_gizmo.slang`, using interpolated `ringLocalXY` and Jacobian-based pixel distance (`ddx`/`ddy`). Trackball fill uses `discFillAlpha` on the same quad path (`kStrokeSdfDisc = -3`).

Translate center (`style 2`) still tessellates a 12-gon fan (`vsCenter`, 72 vertices). Arrow stems (`style 0`) use `emitPolyline` with screen-space NDC extrusion and Blender `smoothline` fragment AA — a different coordinate space than SDF rings.

A prior attempt to render the white outer ring in screen-pixel space caused the ring to drift relative to the gizmo when the camera moved. **All new work MUST stay on local 3D-anchored SDF quads.**

## Goals / Non-Goals

**Goals:**

- Center disc: smooth filled circle, pivot-locked, same visual intent as Blender view-align disc.
- Arrow stem: smooth line along local +Z, fixed pixel width via Jacobian, cone mesh unchanged.
- CPU TDD mirrors in `gizmo_sdf_aa` for disc and segment alpha.
- Vertex count reduction for center (72 → 6).

**Non-Goals:**

- Scale stem/box SDF, translate plane border SDF, removing `emitPolyline` dead code.
- Changing `TransformGizmoUniformData` layout (288 bytes).
- Changing `transform_gizmo_pick.cpp` or hover logic.
- Screen-pixel or NDC-circle fragment shaders.

## Decisions

### 1. Reuse `ringLocalXY` / `ringMajorR` varyings for segment coords

**Choice:** Store segment sample position in `ringLocalXY` as local XZ `(x, z)`; endpoints hardcoded in fragment for arrow stem `(0,0)→(0, stemLen)`.

**Alternatives:** New varyings (requires pipeline layout change); `ubo.quad_layout` packing (overloads semantics).

**Rationale:** Zero uniform layout change; arrow stem endpoints are constant in local mesh space.

### 2. `kStrokeSdfSegment = -4.0` stroke marker

**Choice:** Fragment order: `< -3.5` segment, `< -2.5` disc, `< -1.5` ring.

**Rationale:** Keeps existing `-2` ring and `-3` disc markers; clear threshold ordering.

### 3. `discFillAlphaSolid` for center vs `discFillAlpha` for trackball

**Choice:** Center uses `saturate(1.25 - pxDist)`; trackball keeps faint `saturate(0.75 - pxDist)`.

**Rationale:** Center must read as opaque disc; trackball stays ~5% alpha overlay.

### 4. CPU helpers before shader changes (TDD)

**Choice:** `gizmo_sdf_aa_test.cpp` validates `sdSegment2d`, `discFillAlpha`, `segmentLineAlpha` before shader edits.

**Rationale:** Matches `outline_aa` / `gizmo_polyline_aa` pattern; shader-only regressions are hard to catch in CI.

### 5. Subagent-Driven execution

**Choice:** Implement via `/opsx:apply` with one subagent per task group (CPU helpers → center → arrow stem → validation).

**Rationale:** User-selected; isolates shader vs C++ changes for review.

## Risks / Trade-offs

| Risk | Mitigation |
|------|------------|
| Center disc too soft or too sharp | Tune `discFillAlphaSolid` edge_px; visual QA in translate mode |
| Arrow stem width differs from old polyline | Match `halfWLocal` from `line_width_px` + `kPolylineSmoothWidth`; compare side-by-side |
| Segment quad bbox too tight → gaps | `pad_scale` on `vsSdfSegmentQuad` (default 1.0, increase if QA finds gaps) |
| `outline-gizmo-anti-aliasing` delta conflicts (arrow stem polylines) | This change MODIFIED spec supersedes stem polyline requirement with SDF segment |

## Migration Plan

1. Land CPU helpers + tests (no visual change).
2. Switch center style in shader + `k_center_verts` (visual change, low risk).
3. Switch arrow stem (visual change; verify hover/pick tests pass).
4. Manual QA: orbit camera — center and stems stay on pivot; rotate rings unchanged.

Rollback: revert `transform_gizmo.slang` + `transform_gizmo_shared.h` + new files; no data migration.

## Open Questions

- None blocking Batch 1. Batch 2 scope (plane borders, scale stem) deferred.
