## 1. CPU SDF AA helpers (TDD)

- [x] 1.1 Create `gizmo_sdf_aa.h` / `gizmo_sdf_aa.cpp` with `sdSegment2d`, `discFillAlpha`, `segmentLineAlpha`
- [x] 1.2 Add `gizmo_sdf_aa_test.cpp` and register in `engine/src/tests/CMakeLists.txt`
- [x] 1.3 Add sources to `engine/src/runtime/function/render/CMakeLists.txt`
- [x] 1.4 Run `gizmo_sdf_aa_test` — expect all passed

## 2. Center disc → SDF fill

- [x] 2.1 Set `k_center_verts = k_sdf_ring_verts` in `transform_gizmo_shared.h` (remove `k_center_sides`)
- [x] 2.2 Add `discFillAlphaSolid` in `transform_gizmo.slang`
- [x] 2.3 Route style `2u` to `vsSdfRingQuad(..., kStrokeSdfDisc)`; remove `vsCenter`
- [x] 2.4 Fragment: style `2u` uses `discFillAlphaSolid`; trackball keeps `discFillAlpha`
- [ ] 2.5 Manual QA: translate center smooth circle, pivot-locked on camera orbit

## 3. Arrow stem → SDF segment

- [x] 3.1 Add `kStrokeSdfSegment = -4`, `sdSegment2d`, `segmentLineAlpha`, `vsSdfSegmentQuad` in shader
- [x] 3.2 Replace arrow stem `emitPolyline` branch with `vsSdfSegmentQuad`
- [x] 3.3 Fragment path for `strokeCoord < -3.5` calling `segmentLineAlpha`
- [x] 3.4 Run `gizmo_sdf_aa_test`, `transform_gizmo_aa_test`, `transform_gizmo_hover_test`
- [ ] 3.5 Manual QA: arrow stems smooth and axis-locked; cone tips unchanged; rotate rings unchanged

## 4. Validation

- [x] 4.1 Run full gizmo test suite (`outline_aa_test`, `gizmo_polyline_blender_aa_test`, `transform_gizmo_aa_test`, `gizmo_sdf_aa_test`, `gizmo_dial_sides_test`, `transform_gizmo_hover_test`)
- [ ] 4.2 Visual regression: translate center + arrows + rotate white ring (no camera drift)
- [x] 4.3 Run `openspec validate gizmo-sdf-batch1 --strict`
