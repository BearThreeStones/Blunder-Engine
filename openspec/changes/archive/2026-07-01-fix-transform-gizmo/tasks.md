## 1. P0 — Preserve gizmo mode on selection (option A)

- [x] 1.1 Remove `setTransformGizmoMode(translate)` from `EditorSelectionSystem::setSelection`; keep `requestViewportRedraw()` on valid selection
- [x] 1.2 Verify toolbar sync (`syncTransformToolbarFromEngine`) still reflects engine mode after hierarchy/viewport selection changes
- [x] 1.3 Manual check: Rotate → select another entity → mode stays Rotate, toolbar unchanged

## 2. P1 — Fix translate axis arrow rendering

- [x] 2.1 Reproduce missing arrows: confirm `drawTranslateHandle` runs for `trans_x/y/z` and fade factor is non-zero at test camera angles
- [x] 2.2 Audit `gizmoHandleMatrix` for degenerate bases; fix cross-product fallback if any axis produces NaN/zero scale
- [x] 2.3 Fix `transform_gizmo.slang` `vsArrow` or vertex/draw-count mismatch so arrow geometry renders in clip space
- [x] 2.4 If arrows are sub-pixel, raise minimum arrow thickness (`lineWidthScale` floor or `k_axis_line_width`) without affecting plane/dial styles
- [x] 2.5 Manual check: translate mode shows planes + three colored axis arrows; axis drag still works

## 3. P2 — Scale gizmo visuals and interaction

- [x] 3.1 Add `GizmoDrawStyle::scale_box` (name per implementation) to types, shared counts, and `transform_gizmo.slang` procedural box geometry
- [x] 3.2 Implement `gizmoScaleBoxMatrix` (or extend `gizmoHandleMatrix`) and overlay draw branch for scale mode
- [x] 3.3 Add `pickScaleGizmoHandle` with ray-box tests using existing group scale metrics
- [x] 3.4 Extend `TransformGizmoController` mouse handlers for scale pick, drag, cancel (Escape), and `Entity::setScale` with axis masks
- [x] 3.5 Sync inspector scale fields during scale drag; call `markSceneDirty` / `requestViewportRedraw`
- [x] 3.6 Manual check: Scale mode shows distinct handles; axis and uniform drag update entity and inspector

## 4. Rotate verification and validation

- [x] 4.1 After P0+P1, verify rotate dials render and drag in perspective and orthographic views; fix dial path only if still broken
- [x] 4.2 Run editor build (`cmake --build build/vs2026-debug --target engine_runtime` or project validate script)
- [x] 4.3 Full manual pass: G/R/S hotkeys, toolbar buttons, global/local toggle, selection change under each mode
