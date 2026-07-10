## Why

Batch 1 moved translate center disc and arrow stems onto local 2D SDF + Jacobian AA. Plane handles still use solid mesh quads (`vsPlane` + `emitV`) with hard aliased edges, scale axis stems still use solid stem quads, and the shader retains dead screen-space polyline code (`emitPolyline`, `polylineExtrude`, Blender `smoothline` fragment path) left over from the pre-Batch-1 arrow stem. Batch 2 completes the translate/scale handle unification and removes the unused polyline path to reduce maintenance and visual inconsistency.

## What Changes

- Add rectangle-border SDF AA (`sdBox2d`, `rectRingAlpha`) with CPU mirrors and unit tests in `gizmo_sdf_aa`.
- Migrate `GizmoDrawStyle::plane` from solid filled quad (`vsPlane`) to `vsSdfRectQuad` + `rectRingAlpha` (`kStrokeSdfRect = -5`); layout from existing `ubo.quad_layout`.
- Migrate scale axis **stem** in `vsScaleStemBox` from solid quad to `vsSdfSegmentQuad` (same family as translate arrow stem); scale cube mesh unchanged.
- Extend fragment segment path to support scale-stem endpoints (`kMeshScaleStemStart` → `kMeshScaleBoxOffset`) when `style == scale_stem_box`.
- Remove dead code: `emitPolyline`, `polylineExtrude`, and fragment `strokeCoord ∈ [-1,1]` smoothline branch.
- **No** pick/hover/uniform-layout changes; **no** screen-pixel ring rendering.

## Capabilities

### New Capabilities

_None — extends existing `gizmo-sdf-aa` capability._

### Modified Capabilities

- `gizmo-sdf-aa`: Add CPU mirrors and test scenarios for axis-aligned rectangle border SDF alpha.
- `transform-gizmo`: Plane handles and scale stems SHALL use local SDF AA; supersede any remaining solid-quad / polyline stem assumptions.

## Impact

- `engine/shaders/transform_gizmo.slang` — `vsSdfRectQuad`, `rectRingAlpha`, `kStrokeSdfRect`, scale stem segment, polyline removal
- `engine/src/runtime/function/render/gizmo/gizmo_sdf_aa.{h,cpp}` — `sdBox2d`, `rectRingAlpha`
- `engine/src/tests/gizmo_sdf_aa_test.cpp`
- `engine/src/runtime/function/render/gizmo/transform_gizmo_shared.h` — vertex counts unchanged (plane already 6 verts)
- Depends on Batch 1 (`gizmo-sdf-batch1` archived): `vsSdfSegmentQuad`, `segmentLineAlpha`, `kStrokeSdfSegment`
- May touch `isPolylineGizmoStyle` / docs referencing arrow polyline stems (cleanup only)
