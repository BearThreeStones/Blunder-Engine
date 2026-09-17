# Vulkan timeline semaphore is the CPU–GPU wait primitive

Owner-thread waits still used `VkFence` (viewport in-flight, readback / zero-copy poll, Camera Preview, immediate submit, GPU pick). A later compute queue cannot wait those fences. We decided on one device **timeline semaphore**: graphics submits signal a monotonic value; the owner thread polls or waits that value. No second queue in this slice. Domain: [CONTEXT.md — Timeline semaphore](../../CONTEXT.md).

**Status:** accepted

## Considered Options

- **Keep in-flight and one-shot fences until a compute queue exists** — rejected. Grill A is to prove the wait primitive before any overlapping compute.
- **Fence plus timeline on the same submit** — rejected. Two wait styles; Complexity penalty.
- **Dedicated compute queue in this change** — rejected. No new compute pass to overlap. Pick compute stays on the graphics queue.
- **Optional fence fallback when timeline is missing** — rejected. Missing support fails device create, matching Bindless / `shaderDrawParameters`.
- **D3D12 fence values or Job waits in v1** — rejected. D3D12 already has fence values. Jobs must not wait on GPU ([ADR 0057](0057-task-job-system.md)).
