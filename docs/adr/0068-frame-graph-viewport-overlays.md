# Viewport overlays record through Frame graph execute

The main viewport Scene already records through Frame graph `execute`, but outline, overlay lines, overlay AA, SSAO, screen overlays, and copy still ran as hardcoded order after that `execute`. We decided those post-Scene stages are Passes on the same graph, Copy is the only Sink, and declared graph state matches today’s shader-read handshake (color-writing overlay Passes start with Sampled so the graph does not insert Sampled→ColorAttachment before the callback). Extra overlay images stay inside callbacks. Camera Preview stays after this `execute`. Deferred G-buffer vs lighting as graph Passes are [ADR 0069](0069-frame-graph-deferred-split.md), not this overlay knife. Scene wire and Vulkan recorder: [ADR 0067](0067-frame-graph-viewport-wire.md). Domain: [CONTEXT.md — Frame graph execute](../../CONTEXT.md).

**Status:** accepted

## Considered Options

- **Same graph, overlay Passes, Copy as the only Sink, handshake on color-writing Passes** — chosen. Import still offscreen color/depth and Scene’s shadow. Per-frame reconstruct. Failed execute is fatal. Dummy tests cover handshake, omitted Passes, and Copy as Sink.
- **Split Deferred G-buffer and lighting into graph Passes this slice** — rejected. Grill. Path-owned extra images; `FrameGraphFormat` still has no G-buffer formats.
- **Put Camera Preview / Mesh Preview / Thumbnail on this graph** — rejected. Grill. They stay `ForwardRenderPath::renderFrameTo`. Preview still after this `execute`.
- **Import outline / line / SSAO images; expand `FrameGraphFormat` or `FrameGraphUsage`** — rejected. Grill. Extra images stay inside callbacks. Copy declares Sampled Read only.
- **Declare overlay Write ColorAttachment first** — rejected. Graph would insert Sampled→ColorAttachment before the callback and fight LOAD `initialLayout = SHADER_READ_ONLY`.
- **Fake empty Passes for omitted outline / lines / SSAO** — rejected. Grill. Same `if`s as today’s `tickVulkan`. Line+AA is one Pass; omit the whole Pass when there are no line overlays.
- **Keep Scene as Sink, or keep overlay/copy after `execute` as fallback** — rejected. Two dispatch paths. Copy is the Sink; Scene is not.
- **`FrameGraph::reset()` or Job-scheduled execute** — rejected this slice.
