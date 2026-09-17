# Editor Deferred is a G-buffer Pass then a Lighting Pass

The editor viewport already records overlays and Copy on the same Frame graph as Scene ([ADR 0068](0068-frame-graph-viewport-overlays.md)), but Deferred still ran as one Scene callback wrapping `DeferredRenderPath::renderFrame`. We decided editor Deferred is two Passes on that graph: G-buffer then Lighting. Shadow stays in the G-buffer callback. Post-lighting LOAD (scene overlays + transparent) stays in the Lighting callback. G-buffer images stay path-owned and are not imported. `FrameGraphFormat` does not gain `R16_UINT`. Forward and the Player still use one Scene Pass. Copy stays the only Sink. Camera Preview stays after this `execute`. Domain: [CONTEXT.md — Frame graph pass](../../CONTEXT.md). Path: [ADR 0062](0062-deferred-render-path.md). Overlay wire: [ADR 0068](0068-frame-graph-viewport-overlays.md).

**Status:** accepted

## Considered Options

- **G-buffer Pass + Lighting Pass; extra images stay in callbacks** — chosen. Graph tracks offscreen color/depth and shadow only. G-buffer writes depth then reads Sampled. Lighting reads depth and shadow Sampled, writes color then reads Sampled. Both Passes are built every Deferred tick. Neither is a Sink. No leftover `renderFrame` Scene callback.
- **Three Passes (LOAD separate) or four (shadow on the graph)** — rejected. Grill. Shadow is still inside Forward `recordShadowPass`. LOAD still writes the same offscreen color and would fight the overlay handshake Pass boundary.
- **Import G-buffer as Externals or allocate as Transients** — rejected. Grill. Receiver format is not in `FrameGraphFormat`. Path already owns FIF slots, resize, and `dropGpuTargets`.
- **G-buffer only Write depth, let the graph insert DepthAttachment→Sampled before Lighting** — rejected. Grill. Would fight the G-buffer render pass leaving `DEPTH_STENCIL_READ_ONLY`.
- **Lighting color handshake starts with Sampled** — rejected. Grill. G-buffer does not touch color; a first access of Sampled would invent a barrier before the CLEAR.
- **Keep `renderFrame` as a Scene fallback, or Fake empty G-buffer/Lighting Passes on Forward** — rejected. Two dispatch paths. Fake empty Passes already rejected in ADR 0068.
