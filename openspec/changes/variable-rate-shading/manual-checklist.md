# Manual checklist — variable-rate-shading

One row per User story. Agent QC does not substitute. Only the human confirms.

**Status:** Not run

Windowed `engine_editor` and Play on the DogWalk Project (`E:\Blunder Projects\DogWalk`). Open `assets/Scenes/se-world.scene.asset`. Do not use Test or Sponza. Do not use collision QC `root.scene.asset`. Do not use PR 24 / collision product exe. Do not use PR 36 / open-progress apply as this walk.

| # | User story | Pass |
|---|------------|------|
| 1 | Open DogWalk in windowed `engine_editor`. The Viewport is still clustered deferred (directional + froxel point/spot). With `VK_KHR_fragment_shading_rate`, the lighting fullscreen triangle uses the **previous frame’s Sobel luminance edges** at 1×1 / 2×2: canopy / horizon stays 1×1; large flats (sky, wall, far ground) go 2×2. | |
| 2 | A **rate-mask debug overlay** (article Figure 9.4: one colour for 1×1, another for 2×2) can be toggled on the editor Viewport. Default off, not persisted, not in Player / Preview / Thumbnail. | |
| 3 | With VRS off or with no device extension: shading is today’s 1×1 clustered deferred. Editor and Play still open and orbit. No software VRS path. No FATAL. | |
| 4 | G-buffer, shadows, transparent, outline, pick, SSAO, volumetric fog, and the open-progress overlay still look like today. Receiver ids are not smeared by 2×2, so Light linking does not jump to a neighbour MeshRenderer. | |
| 5 | Play (`engine_player`) on the same scene is deferred clustered lighting. With the extension, that lighting triangle uses the same image-based VRS. The Player has no mask overlay. | |
| 6 | Camera Preview, Mesh Preview, and Scene Thumbnail / Capture are deferred (same lighting-pass family VRS can attach to), not Forward opaque. Placement Preview stays Forward and is not rate-shaded. Headless / CLI / MCP do not need the extension to run. | |
| 7 | The walk is DogWalk `se-world.scene.asset`. Test and Sponza are not the demo Project. 138 Spot, a profiler “how much faster” number, and the dog walking are not required. Do not touch PR 24 or PR 36. | |
