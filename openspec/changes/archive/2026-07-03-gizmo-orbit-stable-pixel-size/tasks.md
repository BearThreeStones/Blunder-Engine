## 1. Pixsize / depth split

- [x] 1.1 Add `projection` to `TransformGizmoScaleContext`; document pixsize uses projection-only, depth uses view×projection
- [x] 1.2 Change `computeViewportPixsize()` to take `projection` matrix (not `view_projection`) for column len_px
- [x] 1.3 Keep perspective depth as `|clip.w|` from `view_projection × pivot`; ortho path unchanged
- [x] 1.4 Wire `projection` + `view_projection` in `transform_gizmo_overlay.cpp` and `transform_gizmo_controller.cpp`

## 2. Regression guards

- [x] 2.1 Add debug-only orbit stability check: log if `group_scale` at fixed pivot varies >5% across yaw/pitch at constant distance
- [x] 2.2 Verify existing `debugLogGizmoScaleParity` still passes after change

## 3. Build and validation

- [x] 3.1 Build `engine_runtime` and `engine_editor` Debug
- [x] 3.2 Manual: perspective orbit (pivot centered) — gizmo overall diameter stable; arrows + rings scale together
- [x] 3.3 Manual: scroll zoom — screen diameter still ~constant
- [x] 3.4 Manual: Persp ↔ Iso toggle — diameter within 15%
- [x] 3.5 Manual: small docked viewport — cap still applies; no regression on polyline dial widths
