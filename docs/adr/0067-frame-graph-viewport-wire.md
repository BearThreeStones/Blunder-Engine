# Viewport Scene records through Frame graph execute

GPU Execute can play barriers onto a recorder, but `tickVulkan` still hardcoded the editor viewport Scene as `DeferredRenderPath::renderFrame` or `ForwardRenderPath::renderFrame`. We decided the main viewport Scene is one Frame graph Pass whose callback still calls the Forward path, and that `VkImageLayout` mapping lives on a Vulkan Frame graph recorder outside `frame_graph.h`. Overlays, SSAO, copy, and Camera Preview stay after `execute` in this slice. Post-Scene overlay and copy Passes are [ADR 0068](0068-frame-graph-viewport-overlays.md). Editor Deferred is a G-buffer Pass then a Lighting Pass ([ADR 0069](0069-frame-graph-deferred-split.md)), not one Scene wrapping `renderFrame`. Domain: [CONTEXT.md — Frame graph execute](../../CONTEXT.md). GPU Execute: [ADR 0066](0066-frame-graph-gpu-execute.md).

**Status:** accepted

## Considered Options

- **One Scene Pass wrapping `renderFrame`, Vulkan recorder, overlays after execute** — chosen. Import current offscreen color/depth and the shadow map as Externals. Per-frame graph reconstruction (no reset API). Failed execute is fatal; no bypass branch.
- **Split Deferred G-buffer and lighting into graph Passes this slice** — rejected. Grill. `FrameGraphFormat` has no `R16_UINT`; G-buffer is path-owned extra images; lighting already has PRIMARY barriers.
- **Expand `ICommandList` with `pipelineBarrier`** — rejected. It only has `begin` / `end` / `submit`. ADR 0066 already rejected this for GPU Execute.
- **`vulkan.h` / layout mapping on the graph** — rejected. `frame_graph_test` stays device-free. Mapping belongs on the Vulkan recorder.
- **Put overlay / SSAO / copy on the graph this slice** — rejected. Grill. Next knife after Scene execute is proven.
- **Skip External import; Sink with no resource accesses** — rejected. The graph would not track the offscreen or shadow.
- **Sticky compile until resize** — rejected this slice. Double-buffered slots change the import pointer every tick; the graph has no reset API.
- **Keep the old `tickVulkan` path branch as fallback** — rejected. Two dispatch paths.
