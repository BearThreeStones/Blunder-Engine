## Context

See proposal.md for why. Viewport Scene wire: [ADR 0067](../../../docs/adr/0067-frame-graph-viewport-wire.md). GPU Execute: [ADR 0066](../../../docs/adr/0066-frame-graph-gpu-execute.md). Barrier plan: [ADR 0065](../../../docs/adr/0065-frame-graph-barrier-plan.md). Deferred internals: [ADR 0062](../../../docs/adr/0062-deferred-render-path.md). Domain: [CONTEXT.md — Frame graph execute](../../../CONTEXT.md). This slice: [ADR 0068](../../../docs/adr/0068-frame-graph-viewport-overlays.md).

`tickVulkan` already calls `recordViewportSceneGraph` after `vkBeginCommandBuffer` and `begin_sync`. Outline, Line+AA, SSAO, screen overlays, and copy still run after that `execute`. Offscreen color leaves Scene in a shader-read handshake (`initialLayout = SHADER_READ_ONLY` on later LOAD passes). Camera Preview still uses `ForwardRenderPath::renderFrameTo` after that work. Per-frame `FrameGraph` reconstruction, fail-closed External-only allocate, and fatal execute stay. `frame_graph.h` still has no `vulkan.h`. `frame_graph_test` stays Dummy.

## Goals / Non-Goals

**Goals:**

- Same per-tick viewport graph as ADR 0067, with post-Scene Passes: optional Outline, optional Line+AA, optional SSAO, Screen overlays when `OverlaySystem` exists, Copy as the only Sink.
- Scene and color-writing overlay Passes declare the existing shader-read handshake so the graph does not insert Sampled→ColorAttachment before those callbacks.
- Dummy, device-free tests for handshake, omitted Passes, and Copy as Sink.

**Non-Goals:**

- G-buffer / lighting as graph Passes, Camera Preview / Mesh Preview / Thumbnail on the graph, import outline / line / SSAO images, `FrameGraphFormat` or `FrameGraphUsage` expansion, Fake empty Passes, two `tickVulkan` dispatch paths, `FrameGraph::reset()`, Job-scheduled execute, GPU pick on this graph, expanding `ICommandList`, Vulkan Transient allocate.

## Decisions

1. **Same graph, Copy is the only Sink**  
   Extend `recordViewportSceneGraph` (rename to `recordViewportGraph`). Scene is no longer `markSink`. Copy is last and only Sink. Camera Preview stays after this `execute`.  
   *Alternatives:* keep Scene as a second Sink (DCE would keep overlays even when Copy is omitted; Grill: Copy always runs, Scene is not a Sink); split overlays onto a second graph.

2. **Conditionals match today’s `tickVulkan` `if`s**  
   Outline only when `hasActiveOutline()`. Line+AA only when `hasActiveLineOverlays()`. SSAO only when `ssao_enabled`. Screen overlays every frame when `OverlaySystem` exists. Copy always when this tick records. Do not add empty Passes for omitted stages. `begin_sync` stays before Setup so those flags are valid.  
   *Alternatives:* always add Outline/Line/SSAO and no-op inside (Fake empty Passes); two dispatch paths (graph vs leftover `tickVulkan` order).

3. **Color handshake on Scene and color-writing overlay Passes**  
   Same Pass: Read Sampled, Write ColorAttachment, Read Sampled (Scene: Write ColorAttachment then Read Sampled; first access stays the Write so Scene can still be the producer). Overlay Passes start with Sampled so entering state is Sampled, not ColorAttachment. Line+AA is one Pass; when built it always declares the handshake, including when AA is off, so DCE cannot drop line recording. Do not strip overlay/path-internal barriers or `initialLayout = SHADER_READ_ONLY`.  
   *Alternatives:* declare overlay Write ColorAttachment first (graph would insert Sampled→ColorAttachment before the callback and fight LOAD `initialLayout`); strip internal barriers and let the graph own layout.

4. **Depth handshake is Sampled after Scene; SSAO first depth access is Sampled**  
   Scene Writes DepthAttachment then Reads Sampled. SSAO Reads depth as Sampled. Read→Read, no graph barrier before SSAO. SSAO internals still sample depth. Do not import extra images.  
   *Alternatives:* first-access SSAO as DepthAttachment (would insert a graph barrier and hit the recorder’s Sampled→depth layout row); import SSAO/outline targets.

5. **Copy declares Sampled Read only**  
   Callback stays today’s zero-copy `transitionToShaderRead` vs CPU `TRANSFER_SRC` copy then shader-read. No new `FrameGraphUsage`. Compile still grows a Write→Read edge from the previous color Write to Copy, so Scene and overlays stay live. Leaving state after handshake is Sampled, so Copy’s entering Sampled is Read→Read and is not a GPU barrier.  
   *Alternatives:* add Transfer usage; let the graph insert ColorAttachment→Sampled before Copy and drop the callback transitions.

6. **Failed execute stays fatal; External-only allocate stays**  
   No leftover overlay/`renderFrame` branch after a failed execute. Allocator still fails closed if `create*` is called. Per-frame reconstruct, no `reset()`. Vulkan recorder unchanged: still `pipelineBarrier` only.  
   *Alternatives:* skip the frame; fall back to hardcoded overlay order.

7. **ADR 0068**  
   0067 stays the Scene wire and Vulkan recorder. 0062 stays G-buffer vs lighting as path internals.  
   *Alternatives:* extend 0067 (hides the overlay knife); rewrite 0062 so Deferred “is” the graph.

## Risks / Trade-offs

- [Handshake leaving Sampled means Copy often has no GPU graph barrier] → Compile edge still keeps writers live. Callback still does today’s shader-read / copy transitions. Dummy asserts no ColorAttachment barrier *before* overlay callbacks, not that Copy has a barrier.
- [Vulkan recorder maps depth Sampled to `SHADER_READ_ONLY`, not `DEPTH_STENCIL_READ_ONLY`] → This knife avoids firing that row: Scene leaves depth Sampled; SSAO first-access is Sampled. Do not add a Pass whose first depth access is DepthAttachment.
- [Empty External plan on ticks with no Usage change across Passes] → Overlay Writes are same-Pass. Viewport may still never call `pipelineBarrier` in the editor. Dummy still covers the handshake. Mapper stays unit-tested from ADR 0067.
- [Per-frame compile of more Passes] → Still one External-only graph. Accept CPU cost instead of a reset API.
- [`frame_state.shading.ssao_enabled` is false today] → SSAO Pass is omitted until that flag is true. Do not change SSAO enable this knife.

## Migration Plan

1. Update Scene accesses; add overlay/copy Passes; move today’s overlay/copy recording into callbacks; leave Camera Preview after execute; Dummy tests; docs (CONTEXT, ADR 0068, render-pipeline). Keep `frame_graph_test` Dummy.
2. No shader, Preview, Player-deferred, or format-catalog migration.
3. Rollback: restore Scene as Sink and overlay/copy after `execute`; drop overlay Passes from Setup.

## Open Questions

None.
