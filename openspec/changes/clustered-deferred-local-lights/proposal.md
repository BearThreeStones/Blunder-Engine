## Why

The editor Viewport shades every MeshRenderer from at most 8 Light-enabled lights (`scene-light-component` "Light evaluation cap"), taken in stable EntityId order. A Sponza-class scene with many point/spot lights past that cap simply drops the rest — authors cannot see or light-tune scenes with realistic light density. A deferred GBuffer (GPU-driven / frame-graph, currently uncommitted on the Windows tree) is landing under a separate change; this proposal stacks a GPU-built 3D light grid ("froxel" grid) on that GBuffer so the Viewport can shade point and spot lights per-pixel from a bounded per-froxel list instead of a single global cap, while directional lighting stays a fullscreen deferred pass. This unblocks many-local-lights scenes in the editor without clustered shadows, area/IES lights, or translucency clustering.

## What Changes

- Add a per-frame GPU compute pass that fills a 3D froxel grid over the Viewport's GBuffer: screen tiles × exponential-depth Z slices (defaults **64 px** tiles, **32** Z slices — Unreal Forward Light Grid topology). XY only (2D tile) or CPU-side fill are explicitly out.
- Assign visible point and spot lights into every froxel they overlap in that same compute pass. Directional lights are never written into the grid.
- Cap each froxel at **64** assigned lights. Overflow lights past the cap for that froxel are dropped for that froxel only (not culled globally) and the drop is recorded via a log line and a viewport stat; no unbounded linked list or per-froxel dynamic allocation this slice.
- Add a GBuffer lighting pass for the editor Viewport that, per shaded pixel, looks up that pixel's froxel from screen position + reconstructed view depth and shades only from that froxel's point/spot list. Directional lighting continues through the existing fullscreen deferred pass, unchanged.
- Add a Viewport-only debug overlay toggle: an occupancy heatmap that colors each froxel's screen tile by that froxel's assigned-light count (post-cap), for accepting/rejecting the clustering behavior visually.
- **Modify** the existing "Light evaluation cap" requirement in `scene-light-component` so the flat 8-Light-enabled-per-MeshRenderer cap is scoped to the non-clustered forward shading paths (Player, Camera Preview, Placement Preview, Mesh Preview, Scene Thumbnail). The editor Viewport's clustered GBuffer lighting path is exempt from that 8-light cap; it is instead bounded by the per-froxel cap of 64 introduced here.

Out of scope for this change: clustered shadows (point/spot stay unshadowed in the clustered path), area/IES lights in the grid, translucency clustering, and Unreal `Froxel::` visibility/VSM occupancy (name collision only — unrelated system). This change does not touch, amend, or depend on completion of the separate `gpu-driven-rendering` change; it only assumes that change's deferred GBuffer exists to shade against. This change does not propose a new GBuffer or a new forward path.

## User stories

1. As an editor user lighting a Sponza-class scene with dozens of point and spot lights, I open the scene in the Viewport and see every light contributing where it overlaps geometry — not just the first 8 — while the sun (directional) still lights the whole scene through the existing fullscreen pass.
2. As a renderer engineer, I trust that the Viewport's light assignment happens once per frame on the GPU as a compute pass over 3D froxels (screen tiles × exponential depth slices, 64 px / 32 slices by default) — not a 2D-tile-only grid and not a CPU loop that would stall the main thread as light count grows.
3. As a renderer engineer debugging a dense-light scene, I know any single froxel can hold at most 64 lights; past that, extra lights are dropped for that froxel only, and I can see it happened from a log line or stat instead of silent corruption or an ever-growing linked list.
4. As an editor user validating the clustering itself, I toggle a Viewport overlay that paints each froxel's screen tile by how many lights landed in it, so I can visually confirm hot spots and accept that clustering is behaving before trusting the lit result.

## Capabilities

### New Capabilities
- `viewport-light-froxel-grid`: per-frame GPU compute build of the 3D screen-tile × exponential-Z-slice froxel grid; point/spot assignment; 64-lights-per-froxel cap with overflow drop + log/stat.
- `viewport-clustered-gbuffer-lighting`: editor Viewport GBuffer lighting pass that shades point/spot from the pixel's froxel list; directional stays on the existing fullscreen deferred pass.
- `viewport-light-occupancy-heatmap`: Viewport toggle overlay that colors froxel screen tiles by post-cap assigned-light count.

### Modified Capabilities
- `scene-light-component`: "Light evaluation cap" requirement is scoped to non-clustered forward shading paths; the editor Viewport's clustered GBuffer lighting path uses the new per-froxel cap instead of the flat 8-light cap.

## Impact

- Editor Viewport render path: new compute pass (froxel fill) + new/changed GBuffer lighting pass; reads the deferred GBuffer produced by the in-flight `gpu-driven-rendering` frame-graph work (dependency, not modification, of that change).
- `scene-light-component` spec: one requirement's scope narrows (delta spec only, no change to Light Component authoring, fields, or non-Viewport shading).
- New GPU-resident buffers: froxel light-index buffer (fixed `numFroxels * 64` capacity) and per-froxel count buffer; consumed by both the lighting pass and the heatmap overlay.
- Editor-only viewport debug toggle (heatmap); no Player-facing surface.
- Does not touch Player, Camera Preview, Placement Preview, Mesh Preview, or Scene Thumbnail render paths, which keep the existing forward 8-light cap.
