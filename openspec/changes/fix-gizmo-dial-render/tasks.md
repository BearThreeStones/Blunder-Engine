## 1. Investigate

- [x] 1.1 Confirm rotate branch runs: log or breakpoint `recordGizmoDraw` when `gizmo_mode == rotate` (draw count, `group_scale`, pivot)
- [x] 1.2 RenderDoc capture: verify 3× `vkCmdDraw(288)` for dial style; check depth/blend pipeline state on transform gizmo draws
- [x] 1.3 Compare clip-space extent of plane vs dial vertices at same pivot

## 2. Shader — dial + shared non-plane fixes

- [x] 2.1 Increase `vsDial` `tubeR` and add style 3/4 minimum in `lineWidthScale()`
- [x] 2.2 Verify `vsDial` produces non-degenerate triangles; fix vertex indexing if needed
- [x] 2.3 Re-verify `vsArrow` and `vsScaleBox` visibility with thickened geometry and style-specific floors
- [x] 2.4 Rewrite non-plane styles to quad-strip geometry (same family as `vsPlane`): arrow=18v (3 cross-billboards), dial=288v ring quads, center=12-gon
- [x] 2.5 **ROOT CAUSE FOUND**: `vsDial` used `useB = (corner >= 3u)` instead of `uv.x` from `quadUV()` to select angle. This made corners 0&1 identical and corners 4&5 identical, producing **100% degenerate zero-area triangles** for every segment. Fix: `float a = lerp(ang0, ang1, uv.x)`.

## 3. Pipeline / overlay

- [x] 3.1 Confirm transform gizmo pipeline has `depth_test=false`; fix if RenderDoc shows otherwise
- [x] 3.2 Ensure `setTransformGizmoMode` / `toggleTransformGizmoSpace` always present a fresh offscreen frame (audit zero-copy skip paths)

## 4. Validation

- [x] 4.0 Validation experiment: temporarily draw dials as `GizmoDrawStyle::plane` (large square) — confirmed 3 colored squares visible, proving CPU path + matrices correct
- [ ] 4.1 Manual: select entity → R → three rotation dials visible; drag updates rotation
- [ ] 4.2 Manual: G → planes + arrows; S → scale boxes; mode preserved on selection change
- [x] 4.3 Build `engine_runtime` Debug
