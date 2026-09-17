## 1. Vulkan recorder

- [x] 1.1 Add a Vulkan Frame graph recorder (separate compilation unit, not `frame_graph.h`) that implements `IFrameGraphRecorder::pipelineBarrier` as `vkCmdPipelineBarrier`. Bind a non-owning `FrameGraph*` plus `VkCommandBuffer`. Map Frame graph resource state to `VkImageLayout` / stage / access (Undefined `from` → `UNDEFINED`). Do not begin, end, submit, draw, or `beginRenderPass`. Do not expand `ICommandList`. Wire the new sources in `engine/src/runtime/CMakeLists.txt`.
- [x] 1.2 Add thin non-owning `IGpuTexture` import adapters for the current-slot offscreen color, offscreen depth, and shadow `VkImage`. Expose a shadow depth image getter if `ShadowMapTarget` has none. Do not wrap G-buffer images.

## 2. Viewport Scene wire

- [x] 2.1 In `tickVulkan`, after `vkBeginCommandBuffer`, reconstruct a per-tick Frame graph: import those three Externals (`R8G8B8A8_UNORM` color, `D32_SFLOAT` depth and shadow), add one Sink Scene Pass (Write color ColorAttachment, Write depth DepthAttachment, Read shadow Sampled), `setExecute` to existing `ForwardRenderPath::renderFrame` or `DeferredRenderPath::renderFrame` (editor deferred only when `m_deferred_path` and not Player). Compile, allocate with a fail-closed allocator (no Transients), `planBarriers()`, `execute(recorder)`. Remove the `tickVulkan` path branch that called `renderFrame` outside execute. Failed execute is fatal; no fallback branch.
- [x] 2.2 Leave outline, overlay lines, overlay AA, SSAO, screen overlays, copy/readback, and Camera Preview after that `execute`, in today’s order. Do not move `renderFrameTo` surfaces onto the graph.

## 3. Tests

- [x] 3.1 Keep `frame_graph_test` Dummy and device-free. Confirm it still builds and passes (`build/vs2026-debug`, Debug).
- [x] 3.2 Add a mapping test that includes `vulkan.h`, does not create a `VkDevice`, and checks ColorAttachment / DepthAttachment / Sampled / Undefined `from` / Buffer Storage map to the expected layouts or access masks.

## 4. Docs

- [x] 4.1 Keep [ADR 0067](../../../docs/adr/0067-frame-graph-viewport-wire.md) aligned with the shipped recorder and Scene Pass. CONTEXT: Frame graph recorder (Vulkan implementation), viewport Scene Pass, Avoid (G-buffer as graph Passes this slice). Point [ADR 0066](../../../docs/adr/0066-frame-graph-gpu-execute.md) / [ADR 0065](../../../docs/adr/0065-frame-graph-barrier-plan.md) / [ADR 0062](../../../docs/adr/0062-deferred-render-path.md). Update `docs/agents/render-pipeline.md` Scene dispatch.
