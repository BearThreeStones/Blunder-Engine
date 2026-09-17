## Why

CPU–GPU waits still use `VkFence` (viewport in-flight, readback / zero-copy poll, Camera Preview, immediate submit, GPU pick). A later compute queue cannot wait or signal those fences. Grill locked a device **timeline semaphore** as the owner-thread wait/poll primitive first, with no second queue in this slice.

## What Changes

- Enable timeline semaphores on the Vulkan device (Vulkan 1.2 core or `VK_KHR_timeline_semaphore`). A device that cannot enable the feature fails device create (`LOG_FATAL`), same as Bindless / `shaderDrawParameters`.
- Own one device **timeline semaphore**. Graphics-queue submits that today signal an in-flight or one-shot fence instead signal a monotonic value. The owner thread polls or waits that value (`vkGetSemaphoreCounterValue` / `vkWaitSemaphores`).
- Viewport slot reuse, secondary reset, readback / zero-copy present, Camera Preview present, `endImmediateCommands`, and Hybrid GPU pick use those values. Headless with no device allocates nothing.
- Unchanged visuals, pass order, secondary recording, Bindless, Slint Present, and pick results. Jobs still must not wait on GPU.

## User stories

1. I open a scene with materials, shadows, SSAO, and editor overlays (grid, outline, gizmos): it looks like today, and the viewport still presents (readback or zero-copy still about one frame behind).
2. I orbit the camera: there is no extra hitch; Bindless fallback then real textures still work.
3. I use Camera Preview and Mesh Preview: both still draw. I click a mesh: GPU pick still returns a result on a later frame without stalling the tick.
4. I quit the Editor or Player while a frame or an immediate upload is still in flight: the process exits and does not hang waiting on the timeline.
5. I start Headless Editor or Headless Player with no Vulkan device: no timeline semaphore is allocated, and the process still starts and exits.

## Capabilities

### New Capabilities

- `timeline-semaphores`: Device-owned Vulkan timeline semaphore; owner-thread poll/wait of monotonic values for viewport, immediate, and GPU pick submits; required device feature; no device means no alloc; no compute queue in v1.

### Modified Capabilities

- *(none — secondary execute, Bindless residency, overlay visibility, Camera Preview chrome, Headless host composition, and pick hit results do not change. Fence vs timeline is the wait primitive, not a new pick or present contract.)*

## Impact

- **Engine:** `VulkanSync` / `VulkanContext` device create and submit; `RenderSystem` slot reuse and Camera Preview poll; `UIViewportBridge` readback poll; Hybrid GPU pick fences; `endImmediateCommands`. Unused swapchain binary semaphores are not revived for engine present.
- **Tests:** first-party test that no-device initialize allocates nothing, and that timeline values are monotonic / wait-or-poll without a window. Windowed stories 1–4 stay on the checklist.
- **Docs:** CONTEXT glossary; ADR 0060 (written in apply).
- **Non-goals:** dedicated compute queue; moving SSAO / pick / cull onto compute; D3D12; Job waiting on the timeline; dedicated transfer queue; changing pass order or shading; engine-owned swapchain present.
