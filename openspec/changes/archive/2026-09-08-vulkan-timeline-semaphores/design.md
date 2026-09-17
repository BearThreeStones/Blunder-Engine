## Context

See proposal.md for why. Grill locked option A: one device timeline, no compute queue. Today `VulkanSync` holds two in-flight `VkFence`s plus unused swapchain binary semaphores. `RenderSystem::tryBeginRecordingSlot`, Camera Preview present, zero-copy poll, and `UIViewportBridge` poll `vkGetFenceStatus` / `vkWaitForFences`. `endImmediateCommands` creates a one-shot fence and waits. Hybrid GPU pick has its own per-pass fences. Secondary reset happens after the in-flight fence ([ADR 0059](../../../docs/adr/0059-secondary-command-buffers.md)). Jobs must not call RHI or wait on GPU ([ADR 0057](../../../docs/adr/0057-task-job-system.md)). The engine still does not present a swapchain; Slint does. Instance API is Vulkan 1.1 or 1.3.

## Goals / Non-Goals

**Goals:**

- One device timeline semaphore; graphics submits that currently use those owner-thread fences signal a monotonic value instead.
- Same tick policy as today: poll, do not block the tick on an in-flight viewport frame; blocking wait only where `endImmediateCommands` already waits, and on shutdown.

**Non-Goals:**

- A second queue, queue-family ownership, or overlapping compute.
- Dual fence + timeline on the same submit.
- Cleaning unused swapchain binary semaphores unless they block this work (they do not).
- D3D12; Job waits; Texture Loader as a required caller (if that upload path is in the tree, it polls the same timeline instead of a private fence).

## Decisions

1. **One timeline on the Vulkan device (`VulkanSync` / `VulkanContext`), not per pass**  
   There is one graphics queue. A single counter is enough for viewport slots, immediate, and pick to wait each other later. Values are issued on the owner thread.  
   *Alternatives:* one timeline per subsystem (cannot wait pick vs viewport on one object without extra submits); keep fences and add a timeline nobody waits (does not prove the primitive).

2. **Required feature; no fence fallback**  
   Enable `timelineSemaphore` via `VkPhysicalDeviceTimelineSemaphoreFeatures` on the device `pNext` chain. If the instance is Vulkan 1.1, also enable `VK_KHR_timeline_semaphore`. Vulkan 1.2+ core uses the same feature struct (`VkPhysicalDeviceVulkan12Features` when API is 1.3). Missing support is `LOG_FATAL`, matching Bindless.  
   *Alternatives:* optional path that keeps fences (two wait styles); bump every device to 1.2 only (breaks 1.1-only boxes that still have the KHR).

3. **Replace those owner-thread fences; `vkQueueSubmit` fence handle is `VK_NULL_HANDLE`**  
   Signal with `VkTimelineSemaphoreSubmitInfo` on `VkSubmitInfo::pNext`. Viewport slots store the last signaled value (two frames in flight). Poll is `vkGetSemaphoreCounterValue` vs that value. The 1s timeout wait in `tryBeginRecordingSlot` stays a wait on that value, then reset secondaries. Immediate wait uses `vkWaitSemaphores` for the value just signaled. Pick stores a pending value and polls like today’s fence.  
   *Alternatives:* signal both fence and timeline (Complexity penalty); `vkQueueSubmit2` only (needs 1.2 submit path everywhere).

4. **Secondary reset still follows the slot’s GPU completion**  
   After the slot’s timeline value is reached, reset Viewport and Camera Preview secondaries as today. Do not reset while the PRIMARY that executed them is still in flight. Shutdown `vkDeviceWaitIdle` (already on `VulkanContext::shutdown`) before destroying the timeline.  
   *Alternatives:* reset on record (illegal); wait idle every tick (hitch).

5. **Host signal is unused in v1**  
   GPU work signals the counter. The CPU only waits or polls. `vkSignalSemaphore` is not a substitute for a compute queue.  
   *Alternatives:* CPU-signal to “kick” compute (no compute queue in this slice).

6. **D3D12 and Slint Present stay out**  
   D3D12 already has fence values. Slint/Skia still owns HWND Present. Do not wire the engine timeline to the UI swapchain.  
   *Alternatives:* RHI-wide timeline type (no D3D12 caller in Grill); wait Slint’s present on the engine timeline (wrong owner).

## Risks / Trade-offs

- [Poll uses a stale slot value] → Each in-flight slot stores the value from its last submit; reuse waits that slot only, matching today’s per-slot fence.
- [Immediate wait and viewport share the counter] → Monotonic issue-on-submit is enough; immediate wait is a host wait of its own value, not a slot overwrite.
- [Pick and viewport complete out of order] → Pick polls its pending value; viewport polls slot values. Do not treat “any increment” as “this pick is done.”
- [Texture Loader still has a private fence] → Out of required scope. If `submitImmediateCommandsNoWait` loses its fence argument, that caller must take the signaled value and poll it.
- [Jobs later wait the timeline] → Spec forbids it; waits stay on the RHI owner thread.

## Migration Plan

1. Land feature enable + one timeline + tests (no-device, monotonic wait/poll without a window). Switch viewport, immediate, and pick waits. Write ADR 0060 and CONTEXT term.
2. No content or cook-format migration.
3. Rollback: restore in-flight and one-shot fences.

## Open Questions

None.
