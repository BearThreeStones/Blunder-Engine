## 1. Device timeline

- [x] 1.1 Enable `timelineSemaphore` on logical device create (`VkPhysicalDeviceTimelineSemaphoreFeatures` on the `pNext` chain). On Vulkan 1.1 also enable `VK_KHR_timeline_semaphore`. If the feature is missing, `LOG_FATAL` (no fence fallback).
- [x] 1.2 Create one timeline semaphore on the Vulkan device (`VulkanSync` / `VulkanContext`): initial value 0, destroy after `vkDeviceWaitIdle` on shutdown. No device → initialize is a no-op. Issue monotonic values on the owner thread.

## 2. Replace owner-thread fence waits

- [x] 2.1 Viewport in-flight: `vkQueueSubmit` signals the next timeline value (fence handle `VK_NULL_HANDLE`). Store the value per frames-in-flight slot. `tryBeginRecordingSlot`, zero-copy poll, and `UIViewportBridge` poll/wait that value instead of `vkGetFenceStatus` / `vkWaitForFences` / `vkResetFences`. After the slot value is reached, reset Viewport and Camera Preview secondaries as today.
- [x] 2.2 Camera Preview present polls the same slot value. `endImmediateCommands` signals and `vkWaitSemaphores` that value (no one-shot fence). Change `submitImmediateCommandsNoWait` so a caller gets the signaled value; if Texture Loader is in the tree, it polls that value instead of a private fence.
- [x] 2.3 Hybrid GPU pick: drop per-pass fences; signal/poll timeline values so click-to-result stays off the tick.

## 3. Tests and docs

- [x] 3.1 Add `timeline_semaphore_test`: no-device initialize allocates nothing; with a device (or a test double that does not need a window), values are monotonic and wait/poll does not hang. Wire CTest.
- [x] 3.2 Build and run `timeline_semaphore_test` (`build/vs2026-debug`, Debug).
- [x] 3.3 Write [ADR 0060](../../../docs/adr/0060-vulkan-timeline-semaphores.md). Add CONTEXT **Timeline semaphore** under Rendering. Do not add a compute queue, Job waits, or D3D12 fence work.
