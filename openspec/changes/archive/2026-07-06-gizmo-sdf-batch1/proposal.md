## Why

Translate-mode **center disc** and **arrow stems** still use legacy geometry paths (12-sided polygon fan and screen-space polyline extrusion) while rotate rings now use local 2D SDF + Jacobian anti-aliasing. This causes visible faceting on the center disc, inconsistent edge quality on arrow stems, and maintenance of two AA systems. Batch 1 unifies the highest-impact translate handles onto the proven SDF path without repeating the failed screen-pixel ring approach (camera drift).

## What Changes

- Add CPU SDF AA helpers (`gizmo_sdf_aa.{h,cpp}`) with unit tests mirroring shader math.
- Migrate `GizmoDrawStyle::center` from 12-sided fan (72 verts) to `vsSdfRingQuad` + `discFillAlphaSolid` (6 verts).
- Migrate arrow **stem** from `emitPolyline` to `vsSdfSegmentQuad` + `segmentLineAlpha` (`kStrokeSdfSegment = -4`); cone tips unchanged.
- Reduce `k_center_verts` in `transform_gizmo_shared.h` from 72 to 6.
- **No** pick/hover/uniform-layout changes (analytic pick unchanged).

Out of scope (Batch 2): translate plane borders, scale stem SDF, removing dead polyline fragment path, screen-pixel ring rendering.

## Capabilities

### New Capabilities

- `gizmo-sdf-aa`: CPU mirrors and shader contracts for local 2D SDF disc fill and segment stroke alpha (Jacobian + fwidth), tested without GPU.

### Modified Capabilities

- `transform-gizmo`: Center disc and arrow stem rendering SHALL use local SDF AA (same family as rotate rings); supersede polyline-extrusion stem requirement from `outline-gizmo-anti-aliasing` delta for arrow stems only.

## Impact

- `engine/shaders/transform_gizmo.slang` — `vsSdfSegmentQuad`, `segmentLineAlpha`, center style 2 path
- `engine/src/runtime/function/render/gizmo/gizmo_sdf_aa.{h,cpp}` (new)
- `engine/src/runtime/function/render/gizmo/transform_gizmo_shared.h` — `k_center_verts`
- `engine/src/tests/gizmo_sdf_aa_test.cpp`, `CMakeLists.txt`
- `engine/src/runtime/function/render/CMakeLists.txt`
- Reference plan: `docs/superpowers/plans/2026-07-06-gizmo-sdf-batch1-center-arrow.md`
- Execution: **Subagent-Driven** (one subagent per task group, review between tasks)
