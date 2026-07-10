## 1. Rotate trackball overlay

- [x] 1.1 Add `ManipulatorAxis::rot_t` and `GizmoDrawStyle::dial_fill` to types + vertex counts
- [x] 1.2 Implement `vsDialFill` — filled disk, majorR=1.0, in `transform_gizmo.slang`
- [x] 1.3 Add `drawRotateTrackball()` — view-aligned, alpha × 0.05, no clip
- [x] 1.4 Update rotate draw order: trackball → outer ring → axis dials → ghost

## 2. Scale box+stem handles

- [x] 2.1 Add `GizmoDrawStyle::scale_stem_box` shader geometry (stem + box)
- [x] 2.2 Wire `drawScaleHandle` for trans_x/y/z to use `scale_stem_box`
- [x] 2.3 Sync metrics with Blender offsets (stem 0.2–0.8, box half-extent 0.1)
- [x] 2.4 Update scale axis pick thresholds for new geometry

## 3. Scale plane handles

- [x] 3.1 Draw trans_xy/yz/zx plane handles in scale mode (reuse plane style, 0.7× extent)
- [x] 3.2 Extend `pickScaleGizmoHandle` for plane handles
- [x] 3.3 Verify scale drag masks for plane axes (reuse translation mask logic)

## 4. Build and validation

- [x] 4.1 Build `engine_runtime` and `engine_editor` Debug
- [ ] 4.2 Manual: Rotate → faint filled trackball disc under dials and outer ring
- [ ] 4.3 Manual: Scale → box+stem axis handles + plane corners + annulus + center cube
- [ ] 4.4 Manual: Scale plane drag scales two axes; axis drag scales one axis
- [ ] 4.5 Manual: Move/rotate modes unchanged
