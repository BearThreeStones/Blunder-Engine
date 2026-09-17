## Why

The main viewport Scene already records through Frame graph `execute`, but outline, overlay lines, overlay AA, SSAO, screen overlays, and copy still run as hardcoded order after that `execute`. The graph still cannot schedule one real editor GPU frame past Scene.

## What Changes

- Same per-tick viewport Frame graph as [ADR 0067](../../../docs/adr/0067-frame-graph-viewport-wire.md): import offscreen color and depth (and Scene’s shadow Read). Add post-Scene Passes on that graph: optional Outline, optional Line+AA (one Pass), optional SSAO, always Screen overlays, then Copy as the only Sink. Scene is no longer a Sink.
- Color-writing overlay Passes declare the existing shader-read handshake on color: same Pass `Read Sampled` then `Write ColorAttachment` then `Read Sampled`. Scene also leaves color and depth as Sampled Read after its Writes. Graph does not insert Sampled→ColorAttachment before those callbacks. Overlay, line, SSAO extra images stay inside callbacks. Do not import them. Do not expand `FrameGraphFormat` or `FrameGraphUsage`.
- Copy is last and only Sink. Callback is today’s zero-copy `transitionToShaderRead` vs CPU `TRANSFER_SRC` copy then back to shader-read. Graph declares color Sampled Read only. Camera Preview still records after this `execute`. Camera Preview, Mesh Preview, Scene Thumbnail stay `ForwardRenderPath::renderFrameTo`.
- Conditionals match today’s `tickVulkan` `if`s: no outline Pass when no selection outline; no Line+AA Pass when no active line overlays; SSAO Pass only when SSAO is enabled; Screen overlays every frame when `OverlaySystem` exists; Copy always when this tick records the graph. Failed execute stays fatal.
- New [ADR 0068](../../../docs/adr/0068-frame-graph-viewport-overlays.md). [ADR 0067](../../../docs/adr/0067-frame-graph-viewport-wire.md) stays the Scene wire; this knife moves post-Scene stages onto the same graph. [ADR 0062](../../../docs/adr/0062-deferred-render-path.md) still owns G-buffer vs lighting as path internals.

## User stories

1. 默认编辑器 Viewport：outline、gizmo、SSAO、copy 进 UI 看起来和现在一样。这些阶段是同一张 Frame graph 上的 Pass，不是 `execute` 之后写死的顺序。Copy 是 Sink。
2. 没有选中、没有 line overlay、没有 SSAO：那些 Pass 不建（没有 line 就整段 Line+AA Pass 都不建）。Viewport 仍正确。Copy 仍跑。
3. `BLUNDER_EDITOR_DEFERRED=1`：Scene 仍是一个 `DeferredRenderPath::renderFrame` 回调；G-buffer 不上 graph；outline / gizmo 仍不进 G-buffer；overlay / copy 仍在这张 graph 上。
4. Camera Preview、Mesh Preview、Scene Thumbnail 仍 `ForwardRenderPath::renderFrameTo`。Preview 仍在这次 viewport `execute` 之后。
5. `frame_graph.h` 仍无 `vulkan.h`。`frame_graph_test` 仍 Dummy、仍无 Vulkan device。Dummy 证明：color-writing overlay 回调前没有 ColorAttachment graph barrier；省略的 Pass 不是 live；Copy 是 Sink。

## Capabilities

### New Capabilities

- (none)

### Modified Capabilities

- `frame-graph-viewport`: Post-Scene main-viewport stages are Passes on the same Frame graph as Scene. Copy is the only Sink. Extra overlay images stay inside callbacks. Camera Preview and other `renderFrameTo` surfaces stay off this graph.
- `frame-graph`: Dummy, device-free tests cover the overlay handshake Setup (no ColorAttachment graph barrier before a same-Pass Sampled-then-ColorAttachment overlay), omitted Passes not live, and Copy as Sink. Graph headers still have no `vulkan.h`. Viewport overlay dispatch is still not a graph-library responsibility.
- `deferred-render-path`: Editor deferred opt-in still selects which path the Scene Pass callback runs. Overlay and copy Passes on this graph still do not write the G-buffer. G-buffer vs lighting stay inside `DeferredRenderPath`.

## Impact

- **Engine:** `RenderSystem` viewport graph Setup (Scene accesses plus overlay/copy Passes). Existing overlay / SSAO / copy recording moves into Pass callbacks. Vulkan Frame graph recorder stays as shipped in ADR 0067. No new recorder methods. No Transient allocate this knife. `ICommandList` unchanged.
- **Tests:** `frame_graph_test` stays Dummy and device-free (story 5). Stories 1–4 are windowed editor (story 3 with `BLUNDER_EDITOR_DEFERRED=1`).
- **Docs:** CONTEXT Frame graph pass / viewport Sink (Copy, not Scene); ADR 0068; pointer from 0067; `docs/agents/render-pipeline.md` post-Scene order.
- **Non-goals:** Split Deferred G-buffer / lighting into graph Passes; Camera Preview / Mesh Preview / Thumbnail on the graph; import outline / line / SSAO images; expand `FrameGraphFormat` or `FrameGraphUsage`; Fake empty Passes; two `tickVulkan` dispatch paths; `FrameGraph::reset()`; Job-scheduled execute; GPU pick on this graph.
