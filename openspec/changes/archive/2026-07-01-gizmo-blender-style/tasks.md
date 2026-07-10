## 0. Archive prior change

- [x] 0.1 Move `openspec/changes/fix-gizmo-dial-render/` to `openspec/changes/archive/2026-06-17-fix-gizmo-dial-render/`

## 1. Shader — arrow cone tip

- [x] 1.1 Add cone tip geometry to `vsArrow` in `transform_gizmo.slang` (stem along +Z, 8-sided cone at positive end)
- [x] 1.2 Reduce arrow stem width (`stemHalfW`) to Blender-proportioned thin stem
- [x] 1.3 Update `TransformGizmoDrawCounts::k_arrow_verts` and `vertexCountForStyle` to match new vertex count
- [x] 1.4 Sync `TransformGizmoMetrics::k_mesh_stem_radius` / `k_mesh_arrow_length` with shader literals

## 2. Shader — dial, center, scale, shading

- [x] 2.1 Reduce `vsDial` tube width to thin ring proportions; lower `lineWidthScale` floor for styles 3/4
- [x] 2.2 Sync `TransformGizmoMetrics::k_mesh_dial_tube_radius` with shader
- [x] 2.3 Switch fragment shader to flat UI shading (minimal or no Lambert lighting)
- [x] 2.4 Verify `vsCenter` disc radius matches `k_mesh_center_radius`; tune if needed
- [x] 2.5 Verify `vsScaleBox` cube size/offset matches Blender proportions

## 3. Overlay wiring

- [x] 3.1 Route translate center handle (`trans_c`) to `GizmoDrawStyle::center` instead of `plane`
- [x] 3.2 Route scale handles to `GizmoDrawStyle::scale_box` instead of `plane`
- [x] 3.3 Tune plane handle layout constants (`k_plane_quad_layout`) for smaller corner squares
- [x] 3.4 Ensure draw order unchanged: planes → arrows → center (translate); dials (rotate)

## 4. Pick tolerances

- [x] 4.1 Adjust `transform_gizmo_pick.cpp` thresholds for thinner arrows, dials, and scale cubes
- [x] 4.2 Confirm pick still hits cone tip region along axis (not just stem)

## 5. Build and validation

- [x] 5.1 Build `engine_runtime` and `engine_editor` Debug
- [ ] 5.2 Manual: G → cone arrows + small planes + white center disc; drag each handle type
- [ ] 5.3 Manual: R → three thin rings visible; drag rotation; ghost arc on drag
- [ ] 5.4 Manual: S → axis-end cubes + center cube; axis and uniform scale drag
- [ ] 5.5 Manual: selection change preserves mode; gizmo repositions to new pivot
- [ ] 5.6 Visual compare with Blender default transform gizmo at similar zoom level
