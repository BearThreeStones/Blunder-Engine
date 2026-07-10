## 1. CPU rectangle SDF AA helpers (TDD)

- [x] 1.1 Add `sdBox2d` and `rectRingAlpha` to `gizmo_sdf_aa.h` / `gizmo_sdf_aa.cpp`
- [x] 1.2 Extend `gizmo_sdf_aa_test.cpp` with box distance and rect border alpha cases
- [x] 1.3 Run `gizmo_sdf_aa_test` — expect all passed

## 2. Plane handles → SDF rectangle border

- [x] 2.1 Add `kStrokeSdfRect = -5.0`, `sdBox2d`, `rectRingAlpha`, `vsSdfRectQuad` in `transform_gizmo.slang`
- [x] 2.2 Route style `1u` (`plane`) to `vsSdfRectQuad` using `ubo.quad_layout`; remove `vsPlane` solid fill path
- [x] 2.3 Fragment: `strokeCoord < -4.5` calls `rectRingAlpha` with half-extents from `ubo.quad_layout`
- [ ] 2.4 Manual QA: translate + scale plane squares show smooth bordered edges, pivot-locked on orbit

## 3. Scale stem → SDF segment

- [x] 3.1 Replace scale stem solid quad in `vsScaleStemBox` with `vsSdfSegmentQuad` (`kMeshScaleStemStart` → `kMeshScaleBoxOffset`)
- [x] 3.2 Extend fragment segment path (`strokeCoord < -3.5`) to use scale endpoints when `style == 9u`
- [x] 3.3 Run `gizmo_sdf_aa_test`, `transform_gizmo_aa_test`, `transform_gizmo_hover_test`
- [ ] 3.4 Manual QA: scale stems smooth and axis-locked; scale cubes unchanged; rotate rings unchanged

## 4. Remove dead polyline code

- [x] 4.1 Delete `emitPolyline`, `polylineExtrude`, and fragment `strokeCoord ∈ [-1,1]` branch from `transform_gizmo.slang`
- [x] 4.2 Update `isPolylineGizmoStyle` / docs if they still reference gizmo polyline stems
- [x] 4.3 Grep confirms no remaining `emitPolyline` references in engine shaders

## 5. Validation

- [x] 5.1 Run full gizmo test suite (`outline_aa_test`, `gizmo_polyline_blender_aa_test`, `transform_gizmo_aa_test`, `gizmo_sdf_aa_test`, `gizmo_dial_sides_test`, `transform_gizmo_hover_test`)
- [ ] 5.2 Visual regression: translate planes + scale stems + rotate white ring (no camera drift)
- [x] 5.3 Run `openspec validate gizmo-sdf-batch2 --strict`
