## 1. Froxel grid data and compute fill

- [x] 1.1 Confirm the landed (or in-progress) deferred GBuffer's depth/view-reconstruction interface (near/far, view/projection access) is sufficient for a per-pixel view-depth reconstruction, and record any gap against `design.md` - Risks before wiring the froxel lookup.
- [x] 1.2 Add the froxel grid GPU buffers: `froxelLightIndices[numFroxels * 64]` and `froxelLightCount[numFroxels]`, sized from the active Viewport resolution at the default 64 px tile size and 32 Z slices; verify buffer sizes match `ceil(width/64) * ceil(height/64) * 32` at a sample resolution via a unit or debug-print check.
- [x] 1.3 Implement the exponential view-depth-to-Z-slice mapping (and its inverse) as a shared function used by both the fill pass and the lighting pass; verify with a unit test that near-plane slices span a smaller depth range than far-plane slices and that slice indices stay in `[0, 31]`.
- [x] 1.4 Implement the GPU compute pass that, once per rendered Viewport frame, gathers visible Light-enabled Point/Spot lights and writes them into every overlapping froxel using a sphere-of-influence (range) overlap test, in stable light order; verify with a scene of known light placements that expected froxels list the expected lights.
- [x] 1.5 Implement the per-froxel 64-light cap: stop writing once a froxel's list reaches 64, and record an overflow (log line + Viewport stat counter increment) for any light beyond the cap for that froxel; verify with a synthetic scene of 65+ overlapping lights that exactly 64 are kept and the overflow is logged/counted once per dropped light.
- [x] 1.6 Verify directional and area lights are never written into any froxel (assertion or test scanning froxel contents against light-type).

## 2. Clustered GBuffer lighting pass

- [x] 2.1 Implement the editor Viewport's GBuffer lighting pass point/spot term: derive each shaded pixel's froxel from its screen tile and reconstructed view depth (via the shared mapping from 1.3), and accumulate lighting only from that froxel's `froxelLightIndices`/`froxelLightCount`; verify pixels in different froxels with different light lists can shade differently.
- [x] 2.2 Leave the existing fullscreen deferred directional pass untouched; verify (before/after comparison on a scene with only a Directional Light) that directional-only shading is bit-for-bit or visually unchanged by this change.
- [x] 2.3 Verify clustered point/spot contributions apply no shadow term (no shadow sampling added to the clustered path), matching existing Point/Spot non-shadow-casting behavior.
- [x] 2.4 Confirm the clustered lighting pass is wired only into the editor Viewport's deferred path and not into Player, Camera Preview, Placement Preview, Mesh Preview, or Scene Thumbnail; verify by rendering the same scene through each path and confirming only the Viewport differs when point/spot count exceeds 8.
- [x] 2.5 Build a Sponza-class (or similar dense-light) test scene with more than 8 point/spot lights, none sharing a froxel beyond the 64 cap, and verify the editor Viewport shades contributions from more than 8 of them simultaneously.

## 3. Occupancy heatmap overlay

- [x] 3.1 Add a Viewport toggle (off by default, not persisted across editor launch) that enables/disables the occupancy heatmap overlay; verify the toggle state controls overlay draw with no other side effects on shading.
- [x] 3.2 Implement the heatmap draw that colors each froxel's screen-space tile using that froxel's post-cap `froxelLightCount`; verify two froxels with different post-cap counts render visibly different colors, and that a capped (overflowed) froxel renders using 64, not its pre-cap count.
- [x] 3.3 Confirm the heatmap overlay draws only in the editor Viewport (not Player/Camera Preview/Placement Preview/Mesh Preview/Scene Thumbnail) regardless of toggle state; verify via the same per-path rendering check as 2.4.

## 4. Spec-scoped cap change

- [x] 4.1 Locate every forward-shading call site that currently enforces the flat 8-Light "Light evaluation cap" (Player, Camera Preview, Placement Preview, Mesh Preview, Scene Thumbnail) and verify none of them are touched by this change (cap and behavior stay exactly as before).
- [x] 4.2 Verify the editor Viewport path no longer applies the flat 8-light cap and is instead bounded solely by the per-froxel 64 cap, per the `scene-light-component` delta spec's two scenarios (forward-path 9th-light-dropped case unchanged; Viewport 9-light case not dropped).

## 5. Acceptance pass

- [ ] 5.1 Run through `manual-checklist.md` against the built Viewport (all 4 User stories) and record actual results.
- [x] 5.2 Run `openspec validate clustered-deferred-local-lights --strict` (and any project lint/test commands docs/agents/testing.md calls for touched areas) and confirm a clean pass before requesting Human acceptance.
