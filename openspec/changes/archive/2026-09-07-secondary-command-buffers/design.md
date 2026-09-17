## Context

See proposal.md for why. Today every visual pass uses `VK_SUBPASS_CONTENTS_INLINE` on the frame PRIMARY from `VulkanPipeline::getCommandBuffer`. Camera Preview records into that same PRIMARY after the viewport (`RenderSystem::recordCameraPreviewPass`). Mesh Preview records `renderFrameTo` on `beginImmediateCommands` (one-shot PRIMARY) and waits. Barriers and copies already sit outside those render passes. Jobs must not call RHI ([ADR 0057](../../../docs/adr/0057-task-job-system.md)). The immediate pool stays PRIMARY for Texture Loader and pick.

## Goals / Non-Goals

**Goals:**

- One owner-thread secondary pool on the Vulkan device, with distinct in-flight buffers for Viewport, Camera Preview, and Immediate streams.
- Convert shadow, forward (three secondaries in one subpass), outline, overlay lines, overlay AA, SSAO, and screen overlays.
- Extend RHI `beginRenderPass` with a contents flag; keep draw recording on `VkCommandBuffer` as today.

**Non-Goals:**

- Worker-thread or Job recording; per-worker command pools in v1.
- GPU pick / hybrid pick; Texture Loader copy CBs.
- D3D12 bundles; dedicated transfer queue; pass-order or shading changes.

## Decisions

1. **Pool lives on `VulkanContext`, not `VulkanPipeline`**  
   Mesh Preview and the viewport do not share a `VulkanPipeline` instance. The device already owns the immediate PRIMARY pool. A second pool at that same lifetime matches Headless (no context → no alloc).  
   *Alternatives:* hang CBs off `VulkanPipeline` (Mesh Preview cannot see them); a new Context System (duplicates Render lifetime).

2. **Three streams: Viewport, CameraPreview, Immediate**  
   A SECONDARY already executed in the current PRIMARY must not be reset or re-recorded before submit. Camera Preview follows viewport on that PRIMARY, so it needs its own bank (forward opaque + transparent; shadows are off). Mesh Preview’s immediate wait lets Immediate reset after `endImmediateCommands`. Each stream × pass × `k_max_frames_in_flight` is a slot.  
   *Alternatives:* growable allocate-per-record (harder to test isolation); one bank plus `SIMULTANEOUS_USE` (illegal once executed in the same PRIMARY).

3. **One owner-thread `VkCommandPool` with `RESET_COMMAND_BUFFER_BIT`**  
   Command pools are not thread-safe. v1 records only on the owner, so one pool is enough. Future workers each get their own pool; do not share this one. Reset the slot after the PRIMARY fence (`tryBeginRecordingSlot`), matching today’s PRIMARY reset.  
   *Alternatives:* one pool per pass (no gain on one thread); `TRANSIENT` + pool reset only (less precise with three streams).

4. **Forward color: three secondaries, one subpass**  
   Opaque, scene overlays, transparent must stay one render pass (depth + color). Execute them in that order. Viewport/scissor is not inherited without an extension — set it in each secondary.  
   *Alternatives:* one secondary for the whole forward pass (weaker MT prep); split shadow/overlays but leave forward INLINE (fails Grill B).

5. **RHI contents flag only; execute stays Vulkan**  
   Draws already use raw `VkCommandBuffer`. Add `rhi::SubpassContents` to `IOffscreenRenderTarget::beginRenderPass` and the native begin helpers (`ShadowMapTarget`, outline, line pass, AA, SSAO, screen pass). `vkCmdExecuteCommands` stays next to those begins. D3D12 implements the new parameter as Inline-only (ASSERT if Secondary).  
   *Alternatives:* a full secondary RHI on `ICommandList` (no draw API to hang it on); Vulkan-only begin that bypasses RHI (parallel path).

6. **Barriers and copies stay on the PRIMARY**  
   Image layout transitions, shadow-to-shader-read, readback copies, and Camera Preview copies are outside the render pass. Secondaries use `RENDER_PASS_CONTINUE`.  
   *Alternatives:* move barriers into secondaries (illegal inside the pass they continue).

7. **Skip means skip; entered pass always executes ≥1 secondary**  
   If today’s code does not begin a pass, do not begin it. If it does, begin with `SECONDARY_COMMAND_BUFFERS` and execute at least one begun-and-ended secondary (empty is allowed). While converting `tickVulkan`, call `draw_outline` once (it is invoked twice today).  
   *Alternatives:* execute zero secondaries (some drivers reject); keep INLINE when a list is empty (two code paths).

## Risks / Trade-offs

- [Camera Preview reuses viewport secondaries] → Separate CameraPreview slots; tests cover stream isolation without a window.
- [Mesh Preview immediate overlaps an in-flight viewport frame] → Immediate bank is not Viewport/CameraPreview slots.
- [Reset secondaries while GPU still executes them] → Reset only after that frame’s in-flight fence, same as PRIMARY; shutdown `vkDeviceWaitIdle` (or equivalent) before destroy.
- [Missing viewport/scissor in a secondary] → Dynamic state recorded in every drawing secondary.
- [Jobs later used to record] → Spec forbids it; workers need new pools in a later change.

## Migration Plan

1. Land pool + tests (no-device, slot isolation). Convert named passes. Write ADR 0059 and CONTEXT term. Update `docs/agents/render-pipeline.md` so the tick diagram matches.
2. No content or cook-format migration.
3. Rollback: revert pool and restore `INLINE` begins.

## Open Questions

None.
