# Manual checklist — Clustered deferred local lights (Viewport)

Human-run acceptance for the `clustered-deferred-local-lights` change. Requires the (external) deferred GBuffer / frame-graph work to be present in the build; run against a build where that prerequisite is available.

**Status: Not run.**

| # | User story | Acceptance check | Status |
|---|---|---|---|
| 1 | Viewport many local lights — many point+spot lights shade GBuffer pixels from that pixel's froxel list; directional stays fullscreen deferred. | Load a Sponza-class scene with more than 8 point/spot lights (none overflowing a single froxel past 64). Confirm the editor Viewport shades contributions from more than 8 of them simultaneously, and that the directional (sun) light's contribution is unchanged from before this change. | Not run |
| 2 | GPU fill 3D froxels — one compute pass per frame assigns point/spot into screen tiles × exponential depth slices (64 px tiles, 32 Z slices by default). | Confirm (via profiler/capture) that froxel assignment is a single GPU compute dispatch per frame, that the grid has more than one Z slice per XY tile, and that near-camera Z slices are visibly thinner than far Z slices (e.g., via the heatmap or a debug capture). | Not run |
| 3 | Max 64 lights per froxel — overflow dropped with log/stat. | Construct a scene with 65+ point lights overlapping one froxel. Confirm exactly 64 are shaded for that froxel, a log message appears for the drop, and the Viewport's dropped-light stat increments. Confirm an adjacent, non-overflowed froxel is unaffected. | Not run |
| 4 | Occupancy heatmap — Viewport toggle overlay colored by lights-per-froxel, used to accept clustering. | Toggle the heatmap on in the Viewport; confirm froxels with different post-cap light counts render distinguishable colors, an overflowed froxel reads as capped (64) not its raw overlap count, toggling off returns to normal shading, and the overlay never appears in Player/Camera Preview/Placement Preview/Mesh Preview/Scene Thumbnail. | Not run |

## Regression checks (non-Viewport paths unchanged)

- [ ] Player, Camera Preview, Placement Preview, Mesh Preview, and Scene Thumbnail still enforce the flat 8-Light "Light evaluation cap" exactly as before this change (9th affecting light dropped, first 8 in stable EntityId order).
- [ ] No clustered shadows appear for point/spot lights shaded via the froxel-based path (illumination only, consistent with existing non-shadow-casting Point/Spot behavior).
