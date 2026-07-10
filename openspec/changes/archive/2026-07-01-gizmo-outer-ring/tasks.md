## 1. Types & metrics

- [x] 1.1 Add `ManipulatorAxis::rot_c` and `scale_c_outer` to enum and helper functions
- [x] 1.2 Add `GizmoDrawStyle::dial_wire` and `annulus` to enum and vertex counts
- [x] 1.3 Add metrics: `k_rotate_outer_ring_scale_basis` (1.2), `k_scale_outer_ring_scale_basis` (0.2), `k_mesh_outer_ring_radius` (1.0), `k_arc_inner_factor` (6.0)
- [x] 1.4 Move axis dial scale basis to 1.0; assign 1.2 to `rot_c` only in `gizmoScaleBasisForAxis`

## 2. Shader geometry

- [x] 2.1 Implement `vsDialWire` — full ring, majorR=1.0, thin tube, no clip
- [x] 2.2 Implement `vsAnnulus` — inner + outer wire rings (innerR = outerR / arc_inner_factor)
- [x] 2.3 Wire `vertexMain` and update `TransformGizmoDrawCounts`

## 3. Overlay draw paths

- [x] 3.1 Add `drawRotateOuterRing()` — view-aligned matrix, `centerColor()`, `dial_wire`, clip disabled
- [x] 3.2 Add `drawScaleOuterAnnulus()` — view-aligned matrix, `centerColor()`, `annulus`
- [x] 3.3 Rotate branch: draw outer ring first, then axis dials (+ ghost unchanged)
- [x] 3.4 Scale branch: draw annulus (before or after axis cubes; center cube on top)

## 4. Build and validation

- [x] 4.1 Build `engine_runtime` and `engine_editor` Debug
- [x] 4.2 Manual: Rotate → white full outer ring larger than colored semicircular dials
- [x] 4.3 Manual: Scale → white dual concentric circles around center cube
- [x] 4.4 Manual: Orbit camera → outer rings stay view-aligned
- [x] 4.5 Manual: Move/Scale modes unchanged; no draw budget overflow
