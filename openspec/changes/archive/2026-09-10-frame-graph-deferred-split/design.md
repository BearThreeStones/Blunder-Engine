## Context

See proposal.md for why. Overlay wire: [ADR 0068](../../../docs/adr/0068-frame-graph-viewport-overlays.md). Scene wire: [ADR 0067](../../../docs/adr/0067-frame-graph-viewport-wire.md). Path internals: [ADR 0062](../../../docs/adr/0062-deferred-render-path.md). This slice: [ADR 0069](../../../docs/adr/0069-frame-graph-deferred-split.md). Domain: [CONTEXT.md — Frame graph pass](../../../CONTEXT.md).

`recordViewportGraph` already imports offscreen color, depth, and shadow, then adds `viewport.scene` (Forward or Deferred `renderFrame`) plus overlay Passes and Copy as the only Sink. `DeferredRenderPath::renderFrame` still sequences shadow, G-buffer RP, lighting RP, then LOAD on that one callback. G-buffer planes stay path-owned FIF slots (`R8G8B8A8_UNORM` albedo / normal-MR; receiver `R16_UINT`, runtime may fall back to `R32_UINT`). `FrameGraphFormat` still has no `R16_UINT`. Camera Preview still uses `ForwardRenderPath::renderFrameTo` after this `execute`. Per-frame reconstruct, fail-closed External-only allocate, and fatal execute stay. `frame_graph.h` still has no `vulkan.h`. `frame_graph_test` stays Dummy.

## Goals / Non-Goals

**Goals:**

- Editor Deferred: `viewport.gbuffer` then `viewport.lighting` on the same viewport graph; no `viewport.scene`; no `renderFrame` Scene callback.
- Declared accesses match today’s layouts so the graph does not insert DepthAttachment→Sampled before Lighting or Sampled→ColorAttachment before Lighting’s CLEAR.
- Dummy, device-free test for that depth handshake, live order, Copy as Sink, G-buffer not Sink.

**Non-Goals:**

- Import or Transient-allocate G-buffer images; expand `FrameGraphFormat` or `FrameGraphUsage`; shadow or LOAD as their own Passes; Preview on the graph; `renderFrame` fallback; Fake empty Passes on Forward; two `tickVulkan` dispatch paths; `FrameGraph::reset()`; Job-scheduled execute; GPU pick on this graph; recorder G-buffer-format tests; GPU harness of G-buffer images.

## Decisions

1. **Two Passes, not three or four**  
   G-buffer Pass = today’s `recordShadowPass` plus G-buffer RP. Lighting Pass = today’s lighting RP plus LOAD (`cmdBarrierForLoadPass`, `beginLoadRenderPass`, `recordSceneOverlayAndTransparent`, `endLoadRenderPass`). Overlay Passes stay after Lighting.  
   *Alternatives:* three Passes (LOAD separate — LOAD writes the same offscreen color and would fight the overlay handshake Pass boundary); four (shadow on the graph — shadow is still Forward `recordShadowPass`).

2. **G-buffer images stay off the graph**  
   Path still owns FIF slots, resize, and `dropGpuTargets`. Graph still imports only color, depth, and shadow. Do not import G-buffer. Do not allocate Transients. Do not add `R16_UINT`.  
   *Alternatives:* import as Externals; Transient-allocate (receiver format is not in `FrameGraphFormat`).

3. **Declared accesses**  
   G-buffer: Write depth DepthAttachment then Read Sampled; no color; do not declare shadow. Lighting: Read depth Sampled; Read shadow Sampled; Write color ColorAttachment then Read Sampled. Both Passes every Deferred tick, even with no opaques. Neither is a Sink. Copy stays the only Sink. Same-Pass Sampled after the G-buffer depth Write is the leaving state, so Lighting’s first depth access Sampled is Read→Read and is not a graph barrier. G-buffer’s depth Write still grows a compile edge so DCE keeps G-buffer from Copy through Lighting. Lighting’s first color access is Write because G-buffer never touches color.  
   *Alternatives:* G-buffer only Write depth (graph would insert DepthAttachment→Sampled before Lighting and fight `DEPTH_STENCIL_READ_ONLY`); Lighting color handshake starting with Sampled (would invent Sampled→ColorAttachment before CLEAR); mark G-buffer a Sink.

4. **`recordViewportGraph` branch; extract `renderFrame`**  
   Env off or Player: today’s `viewport.scene` + Forward `renderFrame`. Editor Deferred on: do not add `viewport.scene`; add `viewport.gbuffer` then `viewport.lighting`; callbacks call two new record methods split from `renderFrame`. Do not leave `renderFrame` as a Scene fallback. Do not add Fake empty G-buffer/Lighting Passes on Forward. Failed execute stays fatal. Overlay conditionals unchanged.  
   *Alternatives:* keep a Scene wrapper around `renderFrame`; two `tickVulkan` paths; Fake empty Passes on Forward.

5. **Dummy models the Deferred graph, not G-buffer formats**  
   External color+depth, Lighting Reads shadow Sampled. Assert live order G-buffer → Lighting → Copy; Copy is Sink; G-buffer is not; no Dummy barrier whose `before` is Lighting, `from` usage is DepthAttachment, and `to` usage is Sampled. Keep existing overlay handshake Dummy. No device. No `vulkan.h` on `frame_graph.h`.  
   *Alternatives:* GPU harness of G-buffer images; recorder tests of receiver format.

6. **ADR 0069**  
   0068 stays overlay/Copy. 0067 stays Forward Scene wire and Vulkan recorder. 0062 still owns G-buffer images and lighting internals. Bump 0069 to accepted at apply.  
   *Alternatives:* extend 0068 (hides the Deferred knife); rewrite 0062 so the path “is” the graph.

## Risks / Trade-offs

- [Vulkan recorder maps depth Sampled to `SHADER_READ_ONLY`, not `DEPTH_STENCIL_READ_ONLY`] → This knife avoids firing that row before Lighting: G-buffer leaves depth Sampled; Lighting first-access is Sampled. Dummy asserts no DepthAttachment→Sampled barrier before Lighting. Path internals still set `DEPTH_STENCIL_READ_ONLY` after the G-buffer RP.
- [Shadow Write is invisible to the graph] → Same as today’s Scene Read Sampled while `recordShadowPass` writes inside the callback. G-buffer does not declare shadow; Lighting does the Sampled Read. Do not add a graph Write for shadow this slice.
- [G-buffer does not declare color] → Lighting is the first color writer. External has no incoming Undefined row. Callback still CLEARs. Do not start Lighting with Sampled.
- [Per-frame reconstruct of one extra Pass] → Still one External-only graph. Accept CPU cost instead of a reset API.
- [Empty opaque list still builds both Passes] → Matches today’s `renderFrame` (G-buffer CLEAR + lighting background). Do not omit the Passes when there are no opaques.

## Migration Plan

1. Split `DeferredRenderPath::renderFrame` into G-buffer and Lighting record methods; branch `recordViewportGraph`; Dummy test; docs (ADR 0069 accepted, render-pipeline Deferred tree, CONTEXT pointers). Keep `frame_graph_test` Dummy.
2. No shader, Preview, Player-deferred, or format-catalog migration.
3. Rollback: restore one Scene callback wrapping `renderFrame`; drop `viewport.gbuffer` / `viewport.lighting`.

## Open Questions

None.
