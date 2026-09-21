# Image-based VRS hangs off clustered-deferred lighting

Clustered-deferred lighting is a fullscreen triangle into offscreen color (`DeferredRenderPath::recordLightingPass`). We decided Variable Rate Shading for this knife is the Packt ch.9 shape: a **fragment shading rate image** from a **previous-frame Sobel luminance edge**, attached to that lighting render pass with `VK_KHR_fragment_shading_rate`. Rates are **1×1** (`G > 0.1`) and **2×2** only. The lighting shader does not change. Only the lighting RP moves to RenderPass2; SECONDARY contents stay ([ADR 0059](0059-secondary-command-buffers.md)). Player, Camera Preview, Mesh Preview, and Thumbnail convert from Forward to the Deferred Render Path so they share that lighting triangle; VRS is not a second Forward path. Placement Preview stays Forward. Missing extension skips VRS (no FATAL), matching `VK_EXT_mesh_shader`. Domain: [CONTEXT.md — Variable rate shading](../../CONTEXT.md). Path: [ADR 0062](0062-deferred-render-path.md). Dispatch: [ADR 0069](0069-frame-graph-deferred-split.md).

**Status:** proposed

## Considered Options

- **Per-draw pipeline rate or `vkCmdSetFragmentShadingRateKHR` as the source** — rejected. Grill / article: rate follows luminance edges, not a known draw.
- **Per-primitive `PrimitiveShadingRateKHR` (task/mesh)** — rejected. Article mentioned and did not ship; Grill forbids touching mesh shaders for rate.
- **4×4, or 1×2 / 2×1 this slice** — rejected. Article shipped 1×1 / 2×2; higher rates show blocks.
- **VRS on G-buffer, Forward opaque, fog, SSAO, transparent, overlay, pick** — rejected. G-buffer 2×2 smears `R16_UINT` receiver ids (Light linking). Other passes are not this lighting triangle.
- **A second Forward VRS path for Player / Preview / Thumbnail** — rejected. Grill: VRS never runs on Forward. Those surfaces become deferred, then the same image-based VRS attaches.
- **Editor Viewport only (original Grill default 3)** — rejected. Human confirmed **都要**: windowed Viewport **and** Play, plus convert Player / Camera Preview / Mesh Preview / Thumbnail to deferred.
- **Headless-forced extension** — rejected. Linux CI / Headless must start without `VK_KHR_fragment_shading_rate`.
- **Software compute VRS (article Further reading)** — rejected. Article body does not cover it; Grill forbids a second path.
- **Engine-wide dynamic rendering to copy the article’s `VkRenderingInfoKHR` pNext** — rejected. Lighting already uses `vkCreateRenderPass` + SECONDARY. Only that RP upgrades to RenderPass2.
- **FSR image as Bindless, Asset/Cook, or Frame graph Transient** — rejected. Same ownership as G-buffer planes: path-owned FIF slots.
- **Product settings UI / persist the mask overlay** — rejected. ADR 0062 already rejected Deferred settings UI. Overlay matches occupancy heatmap (session toggle, default off).
- **Stack this planning git on PR 24 or PR 36** — rejected. Branch from `main`.
