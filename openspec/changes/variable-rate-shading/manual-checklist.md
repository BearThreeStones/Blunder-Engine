# Manual checklist — variable-rate-shading

One row per User story. Agent QC does not substitute. Only the human confirms.

**Status:** Run 2026-09-21 — Fail (stories 1, 2, 5, 6) / Pass (stories 3, 4, 7). Windowed walk on DogWalk `se-world`. Vulkan GPU was Intel UHD 770 (`VK_KHR_fragment_shading_rate` absent). NVIDIA RTX 3050 is in `nvidia-smi` but has no Vulkan ICD. Evidence: Project Context `internal/vrs-window-walk.md`.

Windowed `engine_editor` and Play on the DogWalk Project (`E:\Blunder Projects\DogWalk`). Open `assets/Scenes/se-world.scene.asset`. Do not use Test or Sponza. Do not use collision QC `root.scene.asset`. Do not use PR 24 / collision product exe. Do not use PR 36 / open-progress apply as this walk.

| # | User story | Pass |
|---|------------|------|
| 1 | Open DogWalk in windowed `engine_editor`. The Viewport is still clustered deferred (directional + froxel point/spot). With `VK_KHR_fragment_shading_rate`, the lighting fullscreen triangle uses the **previous frame’s Sobel luminance edges** at 1×1 / 2×2: canopy / horizon stays 1×1; large flats (sky, wall, far ground) go 2×2. | **Fail** 2026-09-21. Windowed se-world Viewport is clustered deferred. FSR extension missing on Intel; no 1×1/2×2 Sobel. |
| 2 | A **rate-mask debug overlay** (article Figure 9.4: one colour for 1×1, another for 2×2) can be toggled on the editor Viewport. Default off, not persisted, not in Player / Preview / Thumbnail. | **Fail** 2026-09-21. MCP toggle + default-off work. No Figure 9.4 colours without attachment. |
| 3 | With VRS off or with no device extension: shading is today’s 1×1 clustered deferred. Editor and Play still open and orbit. No software VRS path. No FATAL. | **Pass** 2026-09-21. No-extension + `BLUNDER_EDITOR_VRS=0`: 1×1 deferred, orbit, `software_vrs=false`, no FATAL. |
| 4 | G-buffer, shadows, transparent, outline, pick, SSAO, volumetric fog, and the open-progress overlay still look like today. Receiver ids are not smeared by 2×2, so Light linking does not jump to a neighbour MeshRenderer. | **Pass** 2026-09-21. Lighting/gizmos look like clustered deferred se-world. 2×2 smear not exercisable (VRS off). |
| 5 | Play (`engine_player`) on the same scene is deferred clustered lighting. With the extension, that lighting triangle uses the same image-based VRS. The Player has no mask overlay. | **Fail** 2026-09-21. Player ran se-world (`Blunder - se-world - N FPS`), no mask chrome. No usable player framebuffer. No FSR. |
| 6 | Camera Preview, Mesh Preview, and Scene Thumbnail / Capture are deferred (same lighting-pass family VRS can attach to), not Forward opaque. Placement Preview stays Forward and is not rate-shaded. Headless / CLI / MCP do not need the extension to run. | **Fail** 2026-09-21. Camera Preview chrome empty; mesh inspector cropped; thumbnail still grey. Headless MCP runs without extension. |
| 7 | The walk is DogWalk `se-world.scene.asset`. Test and Sponza are not the demo Project. 138 Spot, a profiler “how much faster” number, and the dog walking are not required. Do not touch PR 24 or PR 36. | **Pass** 2026-09-21. se-world only. Did not touch PR 24 / 36. |
