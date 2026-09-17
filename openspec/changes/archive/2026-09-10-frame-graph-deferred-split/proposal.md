## Why

The main viewport already records overlays and Copy through Frame graph `execute`, but editor Deferred still runs as one Scene callback wrapping `DeferredRenderPath::renderFrame`. The graph still cannot see G-buffer vs lighting as two stages, so it cannot schedule one real Deferred GPU frame the way it already schedules Forward Scene then overlays.

## What Changes

- Editor Deferred (`BLUNDER_EDITOR_DEFERRED=1`, not Player): `recordViewportGraph` does **not** add `viewport.scene`. It adds `viewport.gbuffer` then `viewport.lighting`. Do **not** call `DeferredRenderPath::renderFrame`. Extract that body into two record methods. Shadow stays inside the G-buffer callback (`ForwardRenderPath::recordShadowPass`). Post-lighting LOAD (scene overlays + transparent) stays in the Lighting callback. Overlay and Copy Passes stay after Lighting (Forward: after Scene).
- G-buffer images stay path-owned. Do **not** import them. Do **not** Transient-allocate them. Do **not** expand `FrameGraphFormat` or `FrameGraphUsage`. Receiver stays `R16_UINT` (runtime may fall back to `R32_UINT`). Graph still imports offscreen color, offscreen depth, and shadow.
- Declared accesses: G-buffer Write depth `DepthAttachment` then Read Sampled; **no color**; this Pass does **not** declare shadow. Lighting Read depth Sampled; Read shadow Sampled; Write color `ColorAttachment` then Read Sampled. Both Passes every Deferred tick, even with no opaques. Neither is a Sink. Copy remains the only Sink.
- Forward and Player stay one Scene Pass (`viewport.scene` + `ForwardRenderPath::renderFrame`). No Fake empty G-buffer/Lighting Passes on Forward. No leftover `renderFrame` Scene fallback. Failed execute stays fatal.
- New [ADR 0069](../../../docs/adr/0069-frame-graph-deferred-split.md). [ADR 0068](../../../docs/adr/0068-frame-graph-viewport-overlays.md) stays overlay/Copy wire. [ADR 0062](../../../docs/adr/0062-deferred-render-path.md) still owns G-buffer images and lighting internals.

## User stories

1. `BLUNDER_EDITOR_DEFERRED=1`：Viewport 看起来和现在一样。G-buffer 和 lighting 是两个 Pass，不是一个 `renderFrame` Scene。Shadow 仍在 G-buffer 回调里。grid / transparent 仍在 Lighting 的 LOAD 里。Copy 仍是 Sink。Overlays 仍在 Lighting 之后。
2. 环境变量未设（以及 Player）：仍是一个 Scene Pass，Forward `renderFrame`。没有 G-buffer / Lighting Pass。看起来和现在一样。
3. Deferred 且没有选中、没有 line overlay、没有 SSAO：那些 overlay Pass 仍不建。G-buffer 和 Lighting **仍建**。Copy 仍跑。
4. Camera Preview、Mesh Preview、Scene Thumbnail 仍 `ForwardRenderPath::renderFrameTo`。Preview 仍在这次 viewport `execute` 之后。
5. `frame_graph.h` 仍无 `vulkan.h`。`frame_graph_test` 仍 Dummy、仍无 Vulkan device。Dummy 证明：live 顺序 G-buffer → Lighting → Copy；Copy 是 Sink；G-buffer 不是；Lighting 前没有 `from` DepthAttachment、`to` Sampled 的 Dummy barrier。

## Capabilities

### New Capabilities

- (none)

### Modified Capabilities

- `frame-graph-viewport`: Editor Deferred is a G-buffer Pass then a Lighting Pass on the same viewport graph, not a Scene wrapping `renderFrame`. Forward and Player stay one Scene Pass. Overlays still follow that shading (after Lighting when Deferred). Copy stays the only Sink. G-buffer images stay off the graph.
- `deferred-render-path`: Editor Deferred dispatch through Frame graph execute is those two Passes. Path still owns G-buffer images, shadow recording, and post-lighting LOAD. Overlay/copy Passes still do not write the G-buffer.
- `frame-graph`: Dummy, device-free tests cover a two-Pass depth handshake (G-buffer Write DepthAttachment then Read Sampled; Lighting Read Sampled; Copy Sink on color), live order G-buffer → Lighting → Copy, Copy as Sink, G-buffer not Sink, and no DepthAttachment→Sampled Dummy barrier before Lighting. Graph headers still have no `vulkan.h`. Viewport Deferred dispatch is still not a graph-library responsibility.

## Impact

- **Engine:** `RenderSystem::recordViewportGraph` Setup branch for editor Deferred. `DeferredRenderPath` splits `renderFrame` into two record methods used as Pass callbacks. Vulkan Frame graph recorder stays as shipped. No new recorder methods. No Transient allocate. `ICommandList` unchanged.
- **Tests:** `frame_graph_test` stays Dummy and device-free (story 5). Stories 1–4 are windowed editor (stories 1 and 3 with `BLUNDER_EDITOR_DEFERRED=1`). No recorder G-buffer-format tests. No GPU harness of G-buffer images.
- **Docs:** CONTEXT Frame graph pass / Deferred Render Path / G-buffer (already pointed at 0069); bump ADR 0069 to accepted; `docs/agents/render-pipeline.md` Deferred tree.
- **Non-goals:** G-buffer on the graph / Transient / format expand; shadow or LOAD as their own Passes; Preview on the graph; `renderFrame` fallback; Fake empty Passes; two `tickVulkan` dispatch paths; `FrameGraph::reset()`; Job-scheduled execute; GPU pick on this graph.
