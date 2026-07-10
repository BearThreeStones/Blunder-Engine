## 1. Blender pixsize and group scale

- [x] 1.1 Add `computeViewportPixsize()` from projection matrix rows + `max(w,h)` viewfac
- [x] 1.2 Replace `computeView3dPixelSizeNoUiScale` with ortho=`pixsize`, persp=`pixsize×depth`
- [x] 1.3 Extend `TransformGizmoScaleContext` with viewport width; wire overlay + controller
- [x] 1.4 Add optional `effective_gizmo_size` clamp (`min(vp) × 0.35`) in `computeGizmoGroupScale`
- [x] 1.5 Unit-style sanity check: log or debug assert Persp/Iso ratio < 1.15 at same distance

## 2. Screen-space polyline dials

- [x] 2.1 Add `line_width_px` to UBO; document std140 layout assert update
- [x] 2.2 Implement screen-space polyline expansion in `transform_gizmo.slang` for circle/annulus
- [x] 2.3 Route `dial`, `dial_wire`, annulus outer ring to polyline path (not tube quads)
- [x] 2.4 Pass Blender px widths from overlay: rot axis 3.0, outer ring 2.0, annulus 2.0
- [x] 2.5 Restore dial major radius to 1.0 if tube compensation was inflating mesh radius
- [x] 2.6 Verify clip plane discard works with polyline geometry

## 3. Pick and metrics

- [x] 3.1 Update `pickDialRing` threshold for polyline + new pixel_size formula
- [x] 3.2 Sync `TransformGizmoMetrics` comments with Blender line width constants
- [x] 3.3 Remove or reduce `gizmoLineWidthScale` inflation for migrated polyline styles

## 4. Build and validation

- [x] 4.1 Build `engine_runtime` and `engine_editor` Debug
- [ ] 4.2 Manual: Persp ↔ Iso toggle — gizmo screen size stays similar
- [ ] 4.3 Manual: Small docked viewport (≤300px) — gizmo no longer dominates frame
- [ ] 4.4 Manual: Rotation dials ~3px stroke, outer ring ~2px, not chunky tubes
- [ ] 4.5 Manual: Translate/scale/trackball/ghost arc unchanged
- [ ] 4.6 Manual: Dial semicircle clip + drag rotation still work
