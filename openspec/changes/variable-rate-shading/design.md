# Design

## Context

See proposal.md for why. Grill locked 2026-09-20: image-based 1×1 / 2×2 (`G > 0.1` → 1×1); VRS only on clustered-deferred lighting; windowed `engine_editor` Viewport **and** Play; convert Player / Camera Preview / Mesh Preview / Thumbnail to deferred. Decision: [ADR 0074](../../../docs/adr/0074-image-based-variable-rate-shading.md).

Today editor Viewport default is Deferred (`BLUNDER_EDITOR_DEFERRED=0` forces Forward). `recordLightingPass` does `recordFroxelFill` then a color-only CLEAR lighting `VkRenderPass` with SECONDARY fullscreen `deferred_lighting.slang`, then PRIMARY LOAD overlay + Forward transparent. No fragment-shading-rate extension, no RenderPass2, no `vkCmdSetFragmentShadingRateKHR`. Player does not construct `DeferredRenderPath` (`recordViewportGraph` uses Forward Scene). Camera Preview / Mesh Preview / Thumbnail call `ForwardRenderPath::renderFrameTo`. Hi-Z already uses the **previous** frame’s offscreen depth; the rate image uses the same FIF ping-pong. Product Project: `E:\Blunder Projects\DogWalk`. Article contract: Context `docs/implementing-variable-rate-shading.pdf` (Packt ch.9). Do not edit `cursor/scene-collision-bridge-8a89` / PR 24 / PR 36.

## Goals / Non-Goals

**Goals:**

- Image-based VRS on the clustered-deferred lighting fullscreen triangle: Sobel after lighting → FIF rate image → next-frame lighting attachment.
- 1×1 / 2×2 only; lighting shader unchanged; lighting RP RenderPass2 + SECONDARY retained.
- Windowed editor Viewport and Play. Convert Player / Camera Preview / Mesh Preview / Thumbnail to deferred so that lighting pass exists. Placement Preview stays Forward.
- Optional extension; `BLUNDER_EDITOR_VRS=0` force-off; no FATAL; no software VRS; Headless does not require the extension.
- Editor Viewport mask overlay (Figure 9.4 colours), default off, not persisted.

**Non-Goals:**

- Per-draw / per-primitive VRS; 4×4; 1×2 / 2×1; software VRS; SpecConstants.
- VRS on G-buffer / Forward / Placement Preview / shadows / transparent / SSAO / fog / overlay / pick.
- Engine-wide dynamic rendering; D3D12 VRS; Bindless FSR; FSR as Asset/Cook/Transient.
- Mesh Loader, flatten, Texture Loader, open overlay, occupancy heatmap behaviour, 138 Spot, profiler numbers.
- PR 24 / PR 36.

## Decisions

1. **Attachment image, not per-draw / per-primitive.**  
   Article lists three Vulkan controls and ships the render-pass shading-rate image because edges are not known per draw. Combiners still run in the lighting SECONDARY (`vkCmdSetFragmentShadingRateKHR`): pipeline size 1×1, combiner KEEP then REPLACE so the attachment wins. Primitive rate stays unused.  
   *Alternatives:* per-draw far-object rate (article mentioned, not shipped); mesh-shader primitive rate (Grill forbid).

2. **Only the lighting fullscreen triangle.**  
   G-buffer 2×2 would smear `R16_UINT` receiver ids and break Light linking. Fog / SSAO / transparent / overlay / pick stay 1×1. `deferred_lighting.slang` does not change. Sobel is a new compute, not a lighting debug path.  
   *Alternatives:* VRS G-buffer (wrong); mix overlay into `deferred_lighting.slang` (article: shading shader unmodified).

3. **Convert Forward surfaces instead of a second VRS path.**  
   Player / Camera Preview / Mesh Preview / Thumbnail become Deferred (G-buffer + clustered lighting) so VRS attaches to the same lighting RP family. Placement Preview stays Forward and is not rate-shaded. `BLUNDER_EDITOR_DEFERRED=0` still only forces the **editor Viewport** to Forward. No `BLUNDER_PLAYER_DEFERRED`. Camera Preview stays unshadowed (ADR 0018), overlay-free, off the viewport graph, recorded after `execute`. Mesh Preview keeps Studio lighting; Scene Thumbnail keeps Light Components / Studio fallback. One `DeferredRenderPath` (or equivalent view) **per OffscreenRenderTarget** — G-buffer + FSR extents match that target. Do not share viewport G-buffer planes with a 480px preview.  
   *Alternatives:* Forward VRS (Grill forbid); leave Play Forward (human rejected default 3).

4. **Previous-frame rate image, path-owned FIF.**  
   Lighting frame N samples slot N−1; after lighting, Sobel writes slot N from current offscreen color (LDR `R8G8B8A8_UNORM` is fine). Clear slot texels to `0` (1×1) before first use. Same ownership as G-buffer / Hi-Z: not Bindless, not Frame graph Transient. Single-frame Thumbnail stills therefore shade 1×1 on that first lighting; allowed.  
   *Alternatives:* same-frame rate (needs lighting twice); graph Transient (G-buffer already path-owned).

5. **Texel size from device properties.**  
   Query `VkPhysicalDeviceFragmentShadingRatePropertiesKHR`. If `{1,1}` is in `[min, max]`, use the article value; else use **min** (often 8×8 / 16×16). Rate image extent is `ceil(color / texel)`. Sobel writes that extent. Do not hardcode a size the device rejects.  
   *Alternatives:* always `{1,1}` (FATAL on some GPUs); always min (wastes the article’s full-res mask on devices that allow it).

6. **Optional extension, env kill switch.**  
   Enable `VK_KHR_fragment_shading_rate` plus `attachmentFragmentShadingRate` (and `pipelineFragmentShadingRate` for the combiner command) only when present. Missing → log, `fragment_shading_rate_enabled = false`, keep today’s lighting RP. `BLUNDER_EDITOR_VRS=0` uses the no-attachment lighting RP even when the extension exists (no Sobel). Do not FATAL device create. Vulkan 1.2 `vkCreateRenderPass2` when available; otherwise `VK_KHR_create_renderpass2` only if that path is already how the device exists — do not take a FATAL dependency on RenderPass2 for devices without FSR.  
   *Alternatives:* require the extension (breaks Linux CI / Headless); software VRS fallback (Grill forbid).

7. **Sobel stays inside the lighting callback.**  
   After `vkCmdEndRenderPass` of lighting, before LOAD overlay (editor Viewport) or before that surface’s copy/readback (Preview / Thumbnail). Graphics queue, Slang, exact-match layout like `froxel_fill`. Sample current color, write storage `R8_UINT` rate image, barrier to `VK_IMAGE_LAYOUT_FRAGMENT_SHADING_RATE_ATTACHMENT_OPTIMAL_KHR` for the next lighting. No `viewport.vrs` Frame graph Pass.  
   *Alternatives:* new graph Pass (Grill default 4 rejected).

8. **Mask overlay is editor Viewport only.**  
   View menu row next to Froxel Occupancy Heatmap. Default off, not persisted, not Project settings. Composite from the rate image after lighting (screen overlay family or a tiny debug blit) — do not fold into `deferred_lighting.slang`. Player / Preview / Thumbnail never draw it. Occupancy heatmap toggle stays independent.  
   *Alternatives:* persist in editor settings (ADR 0062); show on Player (Grill: overlay is the Figure 9.4 instrument, editor-only).

9. **Tests without a VRS GPU.**  
   CPU twin: Rec.709 luminance, Sobel `G = dx² + dy²`, threshold 0.1, texel encode/decode (`size = 2^((texel/4)&3)` × `2^(texel&3)`). `shader_resource_layout_test` for the compute. Device-init test or log-level assertion: missing extension does not FATAL. Player constructs deferred when not the editor Forward hatch. `frame_graph_test` stays Dummy. Windowed Figure 9.4 is Human acceptance.  
   *Alternatives:* require VRS in Merge CI (Linux often has no extension).

10. **Independent of collision-bridge and open-progress git.**  
    Branch from `main` `3e3d42d`. No files from `cursor/scene-collision-bridge-8a89` or `cursor/engine-open-progress-6606`. ADR number **0074** because planning PR 35 already reserved 0073 for the open-progress overlay.  
    *Alternatives:* stack on PR 24 / PR 36 (forbidden).

## Risks / Trade-offs

- **[Risk] Preview / Thumbnail G-buffer cost** → Each offscreen gets its own planes + FSR; Camera Preview longest edge ≤480. Accepted; still cheaper than a second VRS algorithm.
- **[Risk] First Thumbnail still is 1×1** → Previous-frame mask does not exist. Allowed; conversion is the VRS attach point, not still-image speed.
- **[Risk] Device min texel is 8×8 / 16×16** → Sobel writes coarser tiles; overlay still must show two rates. Do not fake `{1,1}`.
- **[Risk] Player lighting looks different after deferred conversion** → Intended: clustered froxel lighting, not the Forward 8-cap. Overlay chrome stays off (existing Player overlay policy).
- **[Trade-off] `BLUNDER_EDITOR_VRS` name on Play** → Grill locked one env kill switch; no second Player env.
- **[Trade-off] Two lighting render passes (with/without FSR)** → Clearer than a dummy all-1×1 attachment when VRS is off.

## Migration Plan

1. Land planning artifacts (this change). No VRS / deferred-preview C++ yet.
2. On apply: device query; lighting RenderPass2; Sobel; convert Player / Preview / Thumbnail to deferred; Viewport overlay; tests; docs.
3. Human walk on Windows DogWalk `se-world.scene.asset` in windowed `engine_editor` and Play. Do not merge this planning PR into PR 24 or PR 36.
4. Rollback: drop FSR attachment and Sobel; restore Player / Preview / Thumbnail Forward `renderFrameTo`; editor Viewport lighting RP stays color-only.

No content format migration.

## Open Questions

None that block specs. Grill defaults locked 2026-09-20 (default 3 rewritten to 都要).
