## Context

See proposal.md for why. GPU Execute: [ADR 0066](../../../docs/adr/0066-frame-graph-gpu-execute.md). `IFrameGraphRecorder::pipelineBarrier` is CPU-only. `tickVulkan` still branches `if (m_deferred_path) deferred->renderFrame else forward->renderFrame`, then overlay / SSAO / copy / Camera Preview. Offscreen color/depth and `ShadowMapTarget` are not `IGpuTexture`. Frame graph has no reset API. Import requires a non-null `IGpuTexture*`. Domain: [CONTEXT.md — Frame graph execute](../../../CONTEXT.md). This slice: [ADR 0067](../../../docs/adr/0067-frame-graph-viewport-wire.md). Deferred internals: [ADR 0062](../../../docs/adr/0062-deferred-render-path.md).

Keep no `vulkan.h` on the graph, `frame_graph_test` linked to `engine_runtime` only with a Dummy recorder, `ICommandList` as `begin` / `end` / `submit` only.

## Goals / Non-Goals

**Goals:**

- Vulkan Frame graph recorder: bind a `VkCommandBuffer` plus the graph (for resolve); `pipelineBarrier` maps `FrameGraphBarrier` to `vkCmdPipelineBarrier`.
- `tickVulkan` Scene: per-frame Setup of one Sink Scene Pass, import current-slot offscreen color/depth and the shadow map as Externals, compile / allocate / `planBarriers` / `execute(recorder)` on the already-begun PRIMARY.
- Scene callback calls existing `ForwardRenderPath::renderFrame` or `DeferredRenderPath::renderFrame`. Overlays stay after execute.

**Non-Goals:**

- G-buffer / lighting as graph Passes, `FrameGraphFormat` expansion, overlay / SSAO / copy on the graph, Preview / Thumbnail on the graph, Player deferred, expanding `ICommandList`, Vulkan Transient allocate, Job-scheduled execute, `FrameGraph::reset()`.

## Decisions

1. **Vulkan recorder is a separate compilation unit, not `ICommandList`**  
   Implements `IFrameGraphRecorder`. Holds a non-owning `FrameGraph*` and `VkCommandBuffer` for the duration of one `execute`. Graph does not store it. Mapping lives here: usage + access → `VkImageLayout` / stage / access masks; Undefined `from` → `VK_IMAGE_LAYOUT_UNDEFINED`. Buffer Storage uses a buffer barrier.  
   *Alternatives:* expand `ICommandList` (rejected in ADR 0066); put `vulkan.h` on the graph; no-op `pipelineBarrier` until overlays land (hides a broken mapper).

2. **One Scene Pass wrapping the existing path `renderFrame`**  
   Shadow, opaque, deferred G-buffer/lighting, scene overlay, and transparent stay inside that callback. Scene Pass Writes offscreen color as ColorAttachment and depth as DepthAttachment, Reads the shadow map as Sampled.  
   *Alternatives:* split shadow / opaque / transparent as graph Passes this slice; split Deferred G-buffer vs lighting (format + path-owned images; next knives).

3. **Per-frame `FrameGraph` reconstruction**  
   Double-buffered offscreen slots change the import pointer every tick. The graph has no clear/reset. Reconstruct a caller-owned graph each viewport tick, then compile / allocate / plan / execute. External-only: allocate must not create Transients (allocator that fails closed if `create*` is called). Empty External plan is Ok.  
   *Alternatives:* sticky compile until resize (needs a reset API or never calling Setup again); `FrameGraph::reset()` this slice.

4. **Thin non-owning `IGpuTexture` imports**  
   Offscreen and shadow are not `IGpuTexture`. Import adapters wrap the current `VkImage` (color, depth, shadow) so compile is not `InvalidImport` and the recorder can resolve an image when a barrier exists. Callbacks still record through the existing path objects, not through resolve. G-buffer images are not imported.  
   *Alternatives:* make `OffscreenRenderTarget` implement `IGpuTexture` (wrong type); skip import and run a Sink with no accesses (graph does not track the targets).

5. **Overlays, SSAO, copy, Camera Preview stay after `execute`**  
   Same PRIMARY, same order as today. Camera Preview / Mesh Preview / Thumbnail stay `renderFrameTo`. Player Scene Pass is Forward and ignores `BLUNDER_EDITOR_DEFERRED`.  
   *Alternatives:* put overlays on the graph this slice; put Camera Preview on the graph.

6. **Failed Scene execute is fatal, no bypass branch**  
   Engine-owned Setup should compile. Do not keep `tickVulkan`’s old `if (deferred) else forward` as a fallback.  
   *Alternatives:* skip the frame; fall back to direct `renderFrame` (two dispatch paths).

7. **ADR 0067**  
   0066 stays GPU Execute on a recorder. 0062 stays G-buffer vs lighting as path internals.  
   *Alternatives:* extend 0066 (hides viewport wire); rewrite 0062 so Deferred “is” the graph.

## Risks / Trade-offs

- [Empty barrier plan this slice] → External first access is not a first-use row. Scene `execute` still runs the callback. The mapper is still real; a Dummy-free mapping test may include `vulkan.h` without creating a `VkDevice`.
- [Path-internal barriers vs graph barriers] → Same-Pass work stays in `renderFrame`. Do not also import G-buffer or the recorder will fight `cmdBarrierForLoadPass`.
- [Per-frame compile] → One Pass, three Externals. Accept the CPU cost instead of a reset API.
- [Import adapters] → Must stay valid through `execute` (stack or tick-scoped). Do not free the underlying offscreen/shadow.

## Migration Plan

1. Add Vulkan recorder + import adapters; wire `tickVulkan` Scene through execute; keep overlays after; extend docs (CONTEXT, ADR 0067, render-pipeline). Keep `frame_graph_test` Dummy.
2. No shader, Preview, or Player-deferred migration.
3. Rollback: restore the `tickVulkan` path branch; drop the recorder from the viewport tick.

## Open Questions

None.
