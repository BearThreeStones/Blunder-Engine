# Secondary command buffers, owner-thread first

Viewport, Camera Preview, Mesh Preview, shadow, overlay, and SSAO recorded draws inline on one PRIMARY command buffer. Multi-thread recording cannot start until those passes execute SECONDARY buffers with inheritance. We decided to convert those named visual passes on the **owner thread** first, with a device-owned **Secondary command buffer pool** split into Viewport, Camera Preview, and Immediate streams. Jobs still must not call RHI ([ADR 0057](0057-task-job-system.md)). GPU pick and Texture Loader copies stay PRIMARY. Domain: [CONTEXT.md — Secondary command buffer](../../CONTEXT.md).

**Status:** accepted

## Considered Options

- **Leave INLINE until a render worker pool exists** — rejected. Grill B is to prove secondary execute on the current passes before any second thread.
- **Record SECONDARY from Job workers in v1** — rejected. Jobs must not call RHI ([ADR 0057](0057-task-job-system.md)). Command pools are not thread-safe; workers need their own pools in a later change.
- **One secondary bank reused by Camera Preview on the same PRIMARY** — rejected. A SECONDARY already executed in the current PRIMARY must not be re-recorded before submit.
- **Hang secondaries off `VulkanPipeline`** — rejected. Mesh Preview does not share that pipeline instance; the device already owns the immediate PRIMARY pool.
- **D3D12 command bundles in this change** — rejected. v1 is Vulkan. D3D12 `beginRenderPass` stays Inline-only.
- **Convert GPU pick and Texture Loader copies** — rejected. Pick is a separate immediate PRIMARY path; copies must stay outside render-pass-continue secondaries.
