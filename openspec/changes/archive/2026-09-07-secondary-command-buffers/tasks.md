## 1. Pool and RHI contents flag

- [x] 1.1 Add `rhi::SubpassContents` and pass it through `IOffscreenRenderTarget::beginRenderPass`. Vulkan maps Inline / Secondary; D3D12 stays Inline and must not be called with Secondary.
- [x] 1.2 Add a Secondary command buffer pool on `VulkanContext`: one owner-thread `VkCommandPool` (`RESET_COMMAND_BUFFER_BIT`), SECONDARY buffers, slots = stream × pass × frames-in-flight (Viewport / CameraPreview / Immediate). No device → initialize is a no-op. Wire the `.cpp` in `engine/src/runtime/function/render/CMakeLists.txt`.
- [x] 1.3 Begin a slot with `RENDER_PASS_CONTINUE` + inheritance (render pass, framebuffer, subpass 0). Reset a stream’s frame slots only after that PRIMARY’s in-flight fence. Destroy only after device idle (or equivalent) on shutdown.

## 2. Shadow and forward

- [x] 2.1 `ShadowMapTarget::beginRenderPass` uses secondary contents when shadow runs; record `drawShadowOpaqueList` into the Viewport (or Immediate) Shadow slot; PRIMARY executes it. Viewport/scissor in that secondary.
- [x] 2.2 Forward color: begin offscreen with secondary contents; record opaque, scene overlays, transparent into three secondaries; `vkCmdExecuteCommands` in that order; set viewport/scissor in each. Camera Preview uses CameraPreview forward slots on the same PRIMARY. Mesh Preview uses Immediate slots on `beginImmediateCommands`.

## 3. Overlay and SSAO

- [x] 3.1 Outline prepass/resolve, overlay lines, overlay AA, and screen overlays: same secondary begin/execute pattern. Skip the pass when today’s code skips it. Call `draw_outline` once from `tickVulkan`.
- [x] 3.2 SSAO AO + composite: secondary contents when SSAO runs; barriers stay on the PRIMARY. GPU pick and Texture Loader copies stay PRIMARY.

## 4. Tests and docs

- [x] 4.1 Add `secondary_command_buffer_test`: no-device initialize allocates nothing; Viewport / CameraPreview / Immediate slot keys do not alias for the same pass and frame. No windowed Slint. Wire CTest.
- [x] 4.2 Build and run `secondary_command_buffer_test` (`build/vs2026-debug`, Debug).
- [x] 4.3 Write [ADR 0059](../../../docs/adr/0059-secondary-command-buffers.md). Add CONTEXT **Secondary command buffer** under Rendering. Update `docs/agents/render-pipeline.md` so the tick diagram matches secondary execute. Do not add worker-thread recording or D3D12 bundles.
