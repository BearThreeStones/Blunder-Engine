# Proposal

## Why

DogWalk’s clustered-deferred lighting fullscreen triangle is the expensive per-pixel pass (froxel lookup + VSM/PCF). Packt *Mastering Graphics Programming with Vulkan* ch.9 lowers fragment rate on that class of pass with an **image-based** shading-rate attachment built from a **previous-frame Sobel luminance edge**. Blunder has the clustered lighting pass and a previous-frame Hi-Z shape, but no `VK_KHR_fragment_shading_rate`, no RenderPass2 lighting attachment, and no rate image. Player / Camera Preview / Mesh Preview / Thumbnail still shade Forward, so they have no lighting triangle to attach VRS to. This change is **fragment rate on clustered deferred lighting**, not light count, not fog, not the open-progress overlay. Decision: [ADR 0074](../../../docs/adr/0074-image-based-variable-rate-shading.md).

## What Changes

- **Image-based VRS only.** `VK_KHR_fragment_shading_rate` shading-rate **image attachment** on the clustered-deferred lighting render pass. Not per-draw `vkCmdSetFragmentShadingRateKHR` as the rate source. Not per-primitive `PrimitiveShadingRateKHR`. Not 4×4. Not 1×2 / 2×1.
- **Rates 1×1 and 2×2.** After lighting, a graphics-queue Sobel compute (Slang, Rec.709 luminance, 3×3, workgroup 16×16 / halo 18×18) writes a path-owned FIF rate image. `G > 0.1` → texel `0` (1×1); else `1 << 2 | 1` (2×2). Next frame’s lighting pass reads the previous slot. First frame is cleared to 1×1.
- **Lighting shader unchanged.** `deferred_lighting.slang` stays. Combiners in the lighting SECONDARY: pipeline 1×1, attachment REPLACE. Only the lighting RP moves to RenderPass2 for `VkFragmentShadingRateAttachmentInfoKHR`. SECONDARY contents stay. No engine-wide dynamic rendering.
- **Missing extension is 1×1.** Query like `VK_EXT_mesh_shader`: log, skip VRS, editor and Play still run. No FATAL. No software compute VRS. Headless / Linux CI MUST NOT require the extension.
- **Windowed editor Viewport and Play.** VRS never runs on Forward. Convert **Player / Camera Preview / Mesh Preview / Thumbnail** (Scene Thumbnail Render and Capture) from Forward to **deferred** so they share the clustered-deferred lighting triangle. Placement Preview stays Forward. `BLUNDER_EDITOR_DEFERRED=0` remains the editor-viewport Forward escape hatch only. `BLUNDER_EDITOR_VRS=0` force-disables VRS on every deferred lighting pass that would otherwise use it. No product settings UI.
- **Editor-only mask overlay.** Viewport View-menu toggle (shape of froxel occupancy heatmap): Figure 9.4 colours, default off, not persisted, not in Player / Preview / Thumbnail.

**Out of scope:** per-draw / per-primitive VRS; 4×4 or 1×2 / 2×1; software VRS; specialization constants / `SUBGROUP_SIZE`; VRS on G-buffer, Forward opaque, Placement Preview, shadows, transparent, SSAO, fog, overlay, pick; a second Forward VRS path; engine-wide dynamic rendering; D3D12 VRS; Bindless / Asset / Cook / Frame graph Transient FSR; Headless-forced extension; Mesh Loader / flatten / Texture Loader / open overlay; `cursor/scene-collision-bridge-8a89` / [#24](https://github.com/BearThreeStones/Blunder-Engine/pull/24); [#36](https://github.com/BearThreeStones/Blunder-Engine/pull/36); Test/Sponza as the demo Project; 138 Spot; profiler “how much faster.”

## User stories

1. Open DogWalk in windowed `engine_editor`. The Viewport is still clustered deferred (directional + froxel point/spot). With `VK_KHR_fragment_shading_rate`, the lighting fullscreen triangle uses the **previous frame’s Sobel luminance edges** at 1×1 / 2×2: canopy / horizon stays 1×1; large flats (sky, wall, far ground) go 2×2.
2. A **rate-mask debug overlay** (article Figure 9.4: one colour for 1×1, another for 2×2) can be toggled on the editor Viewport. Default off, not persisted, not in Player / Preview / Thumbnail.
3. With VRS off or with no device extension: shading is today’s 1×1 clustered deferred. Editor and Play still open and orbit. No software VRS path. No FATAL.
4. G-buffer, shadows, transparent, outline, pick, SSAO, volumetric fog, and the open-progress overlay still look like today. Receiver ids are not smeared by 2×2, so Light linking does not jump to a neighbour MeshRenderer.
5. Play (`engine_player`) on the same scene is deferred clustered lighting. With the extension, that lighting triangle uses the same image-based VRS. The Player has no mask overlay.
6. Camera Preview, Mesh Preview, and Scene Thumbnail / Capture are deferred (same lighting-pass family VRS can attach to), not Forward opaque. Placement Preview stays Forward and is not rate-shaded. Headless / CLI / MCP do not need the extension to run.
7. The walk is DogWalk `se-world.scene.asset`. Test and Sponza are not the demo Project. 138 Spot, a profiler “how much faster” number, and the dog walking are not required. Do not touch PR 24 or PR 36.

## Capabilities

### New Capabilities

- `variable-rate-shading`: Image-based `VK_KHR_fragment_shading_rate` on clustered-deferred lighting. Sobel luminance edges → FIF rate image → next-frame lighting attachment. 1×1 / 2×2 only (`G > 0.1` → 1×1). Lighting shader unchanged. RenderPass2 on the lighting RP only. Optional extension; `BLUNDER_EDITOR_VRS=0` force-off; no FATAL; no software path; no Forward VRS. Editor Viewport mask overlay (default off, not persisted).
- `dogwalk-variable-rate-shading`: Human walk on DogWalk `assets/Scenes/se-world.scene.asset` in windowed `engine_editor` and Play. Mask overlay on the editor Viewport. Not Test/Sponza. Not collision QC `root.scene.asset`. Not PR 24 / PR 36.

### Modified Capabilities

- `deferred-render-path`: Default deferred surfaces expand to Player, Camera Preview, Mesh Preview, and Scene Thumbnail / Capture. Placement Preview stays Forward. `BLUNDER_EDITOR_DEFERRED=0` still forces only the editor Viewport to Forward.
- `viewport-clustered-gbuffer-lighting`: Clustered froxel lighting is the lighting pass on every deferred surface in this change, not editor Viewport only.
- `viewport-light-froxel-grid`: Froxel fill runs for those same deferred surfaces.
- `frame-graph-viewport`: Windowed Player records G-buffer then Lighting on the viewport graph (not a Forward Scene). Camera Preview stays off that graph.
- `play-player`: Play-rule camera color is produced by the Deferred Render Path. Image-based VRS may attach to its lighting pass when the extension is present. Headless still does not require the extension.
- `camera-preview`: Preview image is Deferred into the dedicated offscreen (unshadowed, no Editor Overlays, no VRS mask). Still after viewport `execute`, not a viewport-graph Pass.
- `scene-light-component`: Clustered froxel cap (not the flat 8) applies on deferred clustered surfaces including Player / Camera Preview / Mesh Preview / Thumbnail. Remaining Forward (Placement Preview, editor Viewport with `BLUNDER_EDITOR_DEFERRED=0`) keeps the 8-light cap.
- `shader-resource-layout`: New Sobel compute is exact-match FATAL like `froxel_fill`.

## Impact

- **Engine:** Device query for `VK_KHR_fragment_shading_rate` + attachment feature (mesh-shader pattern). Lighting RP RenderPass2 + FSR attachment when enabled. Path-owned FIF rate images. Sobel compute after lighting RP, before LOAD. Player / Preview / Thumbnail construct and record `DeferredRenderPath` against their offscreens.
- **UI:** One Viewport View-menu toggle next to Froxel Occupancy Heatmap. English. Not persisted. Not Player.
- **Tests:** CPU twin for luminance / Sobel threshold / FSR encoding; `shader_resource_layout_test` for the compute; host gating (missing extension does not FATAL; Player deferred). Windowed Figure 9.4 feel is Human acceptance. Merge CI must stay green without the extension.
- **Docs:** CONTEXT VRS terms; [ADR 0074](../../../docs/adr/0074-image-based-variable-rate-shading.md). Apply later updates `docs/agents/render-pipeline.md`.
- **This planning change** is OpenSpec artifacts + glossary/ADR only — no engine/Slint/Slang C++ until `/opsx:apply`.
