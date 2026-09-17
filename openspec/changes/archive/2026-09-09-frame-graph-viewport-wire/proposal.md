## Why

GPU Execute can play a CPU barrier plan onto a recorder, but `tickVulkan` still hardcodes the editor viewport Scene dispatch (`DeferredRenderPath::renderFrame` or `ForwardRenderPath::renderFrame`). The graph cannot schedule that frame until a Vulkan recorder exists and the viewport calls `execute`.

## What Changes

- Add a Vulkan Frame graph recorder in a separate compilation unit: `pipelineBarrier` maps a `FrameGraphBarrier` to `vkCmdPipelineBarrier`. `frame_graph.h` still has no `vulkan.h`. Not `ICommandList`. Recorder still has no draw / `beginRenderPass` / begin-end-submit.
- `tickVulkan` records one Sink Scene Pass per frame: import the current offscreen color and depth and the Directional shadow map as Externals; compile; allocate (no live Transients this slice); `planBarriers()`; `execute(recorder)` on the already-begun PRIMARY. The Scene callback calls the existing path `renderFrame` (Forward, or Deferred when `m_deferred_path` is set and the host is not Player).
- Overlay, SSAO, screen overlays, copy/readback, and Camera Preview stay **after** that `execute`, in today’s order. Camera Preview, Mesh Preview, Scene Thumbnail / Capture stay `ForwardRenderPath::renderFrameTo`. Player Scene Pass is Forward and still ignores `BLUNDER_EDITOR_DEFERRED`.
- Do not split Deferred into G-buffer / lighting Passes. Do not expand `FrameGraphFormat`. Do not put overlays on the graph.
- New [ADR 0067](../../../docs/adr/0067-frame-graph-viewport-wire.md). [ADR 0066](../../../docs/adr/0066-frame-graph-gpu-execute.md) points here for the viewport wire. [ADR 0062](../../../docs/adr/0062-deferred-render-path.md) still owns G-buffer vs lighting as path internals.

## User stories

1. 不设 `BLUNDER_EDITOR_DEFERRED`，打开编辑器看 Viewport：网格仍 Forward 画进 offscreen，Slint 仍是同一套像素。这一帧的 Scene 是 Frame graph `execute`，不是 `tickVulkan` 里直接 `forward_path->renderFrame`。
2. `BLUNDER_EDITOR_DEFERRED=1`：opaque 仍 G-buffer 再 lighting 进同一张 offscreen；transparent 仍 lighting 之后的 Forward；scene overlay 不进 G-buffer。Scene Pass 回调是 `DeferredRenderPath::renderFrame`，没有第二条绕过 graph 的 `tickVulkan` 分支。
3. Camera Preview、Mesh Preview、Scene Thumbnail 仍 `ForwardRenderPath::renderFrameTo`。Player 仍 Forward，不读编辑器 deferred 开关。
4. Scene `execute` 之后仍是 outline → overlay lines → overlay AA → SSAO → screen overlays → copy/readback。观感与现在同一刀前一致。
5. `frame_graph.h` 仍无 `vulkan.h`。`frame_graph_test` 仍无 Vulkan device、仍 Dummy recorder。Vulkan recorder 是别的编译单元。

## Capabilities

### New Capabilities

- `frame-graph-viewport`: Editor (and Player) main viewport Scene records through Frame graph execute and a Vulkan recorder; one Scene Pass wraps the existing Forward or Deferred `renderFrame`; overlays stay after execute; other mesh surfaces stay off the graph.

### Modified Capabilities

- `frame-graph`: Graph execute still does not include `vulkan.h` or call `vkCmdPipelineBarrier`; a Vulkan recorder outside the graph headers may. Viewport wire is not a graph-library responsibility. `frame_graph_test` stays device-free.
- `deferred-render-path`: Editor deferred opt-in still selects which path the Scene Pass callback runs. That callback is Frame graph execute, not a `tickVulkan` branch that bypasses the graph. G-buffer vs lighting stay inside `DeferredRenderPath`.

## Impact

- **Engine:** new Vulkan Frame graph recorder under `engine/src/runtime/function/render/vulkan_backend/` (or equivalent); `RenderSystem::tickVulkan` Scene dispatch; thin non-owning `IGpuTexture` imports for offscreen color/depth and the shadow map. Path recording (`ForwardRenderPath` / `DeferredRenderPath`) stays. PRIMARY begin/end/submit stay the caller.
- **Tests:** `frame_graph_test` stays Dummy and device-free (story 5). Optional mapping table test may include `vulkan.h` without creating a `VkDevice`. Stories 1–4 are windowed editor (story 2 with `BLUNDER_EDITOR_DEFERRED=1`).
- **Docs:** CONTEXT Frame graph recorder / viewport Scene Pass; ADR 0067; pointers from 0066 / 0062 / 0065; `docs/agents/render-pipeline.md` Scene dispatch.
- **Non-goals:** G-buffer or lighting as graph Passes; expand `FrameGraphFormat`; overlay / SSAO / copy as graph Passes; Camera Preview / Mesh Preview / Thumbnail / Player deferred; expand `ICommandList`; Vulkan Transient allocate; Job-scheduled execute; product settings UI for deferred.
