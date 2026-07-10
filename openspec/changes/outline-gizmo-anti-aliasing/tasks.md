## 1. Outline AA — CPU helper (TDD) [v1 done]

- [x] 1.1 Create `outline_aa.h` / `outline_aa.cpp` with `outlineEdgeCoverage()` and kernel constants
- [x] 1.2 Create `outline_aa_test.cpp`; register in `CMakeLists.txt`
- [x] 1.3 RED: run test before implementation; GREEN: all assertions pass

## 2. Outline AA — shader kernel sync [v1 done]

- [x] 2.1 Update `outline_resolve.slang` `edgeCoverage()` to use `kOutlineEdgeSmoothMin/Max` (0.25 / 2.5)
- [x] 2.2 Extend `outline_aa_test.cpp` for shallow-diagonal coverage; verify GREEN
- [x] 2.3 Build `engine_runtime`; no outline visual regression on prepass path

## 3. Transform gizmo polyline AA — CPU helper (TDD) [v1 done]

- [x] 3.1 Create `gizmo_polyline_aa.h` / `gizmo_polyline_aa.cpp` with `polylineStrokeAlpha()`
- [x] 3.2 Create `transform_gizmo_aa_test.cpp`; register in `CMakeLists.txt`
- [x] 3.3 RED → GREEN: center full alpha, edge zero alpha, mid partial

## 4. Transform gizmo shader AA [v1 done]

- [x] 4.1 Add `strokeCoord` varying to `transform_gizmo.slang` (`emitPolyline` / `emitV`)
- [x] 4.2 Fragment `fwidth` smoothstep alpha for `|strokeCoord| <= 1`
- [x] 4.3 Build `engine_runtime`; run `transform_gizmo_aa_test` + `transform_gizmo_hover_test`

## 5. Docs and validation [v1]

- [x] 5.1 Update `docs/agents/render-pipeline.md` — outline AA + gizmo polyline AA paths
- [x] 5.2 `openspec validate outline-gizmo-anti-aliasing`
- [ ] 5.3 Manual QA: select mesh — smooth outline diagonals; R/S modes — smooth rings; `BLUNDER_EDITOR_RENDER_SCALE=0.75` spot check

## 6. Solid gizmo mesh follow-up [optional v1]

- [ ] 6.1 Solid gizmo mesh edge softening (arrows/planes) — superseded by Task 10 arrow polyline approach

---

## 7. Overlay line AA — CPU mirror (TDD) [v2 Blender parity]

- [x] 7.1 Create `overlay_line_aa.h` / `overlay_line_aa.cpp` — `decodeLineData`, `lineCoverage`, `neighborLineDist` (ref: `overlay_antialiasing.bsl.hh`)
- [x] 7.2 Create `overlay_line_aa_test.cpp`; register in `CMakeLists.txt`
- [x] 7.3 RED → GREEN: decode packed center dist=0; full/zero `lineCoverage`

## 8. Overlay line AA — shader composite [v2]

- [x] 8.1 Port cross-neighbor AA to `overlay_aa.slang` (use `lineDataTexture`; ref: `E:/Dev/Blender/.../overlay_antialiasing.bsl.hh`)
- [x] 8.2 Create `overlay_line_pack.slang` with `packLineData` (ref: `overlay_common_lib.glsl`)
- [x] 8.3 Build `engine_runtime`; manual test with `BLUNDER_EDITOR_OVERLAY_AA=1`

## 9. Grid line MRT + packLineData [v2]

- [ ] 9.1 Align `grid_line.slang` lineData encoding with `packLineData` / `decodeLineData`
- [ ] 9.2 Route grid fine lines through `OverlayLinePass` in `grid_overlay.cpp`
- [ ] 9.3 Manual QA: grid edges smooth with overlay AA enabled

## 10. Gizmo polyline — Blender `smoothline` formula [v2]

- [x] 10.1 Add `polylineStrokeAlphaBlender()` + `kPolylineSmoothWidth=1` to `gizmo_polyline_aa.*`
- [x] 10.2 Create `gizmo_polyline_blender_aa_test.cpp`; RED → GREEN
- [x] 10.3 Update `transform_gizmo.slang` fragment to use Blender formula (`smoothline` from `strokeCoord * halfWidth`)
- [x] 10.4 Run `transform_gizmo_aa_test` + `transform_gizmo_hover_test`

## 11. Arrow stems as polylines [v2]

- [x] 11.1 Change `vsArrow` stem from quad tris to `emitPolyline` (ref: `arrow3d_gizmo.cc`)
- [x] 11.2 Set stem `line_width_px` ≈ 1.0 × pixelsize (`k_plane_line_width` for arrow style)
- [ ] 11.3 Manual QA: translate mode arrow stems vs Blender

## 12. Docs + validation [v2]

- [x] 12.1 Update `docs/agents/render-pipeline.md` — three-layer Blender AA architecture
- [x] 12.2 `openspec validate outline-gizmo-anti-aliasing`
- [ ] 12.3 Manual QA full checklist (outline, grid AA, gizmo R/S/T, render scale 0.75)

## 13. Outline directional detect [optional v3]

- [ ] 13.1 Port `overlay_outline_detect_frag.glsl` edge-case + `pack_line_data` to Slang
- [ ] 13.2 Route outline through line MRT + `overlay_aa.slang` composite
- [ ] 13.3 Manual QA: diagonal outline vs Blender side-by-side
