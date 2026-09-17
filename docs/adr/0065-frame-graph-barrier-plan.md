# Frame graph barrier plan is its own CPU phase

Compile still builds only the CPU DAG. We decided live-Pass resource transitions are recorded in a separate `planBarriers()` after compile, not inside compile, allocate, or execute, and not as `vkCmdPipelineBarrier`. Tests read a CPU list and do not create a Vulkan device. Domain: [CONTEXT.md — Frame graph barrier plan](../../CONTEXT.md). CPU DAG: [ADR 0061](0061-frame-graph-cpu-setup.md). Allocate: [ADR 0064](0064-frame-graph-allocate.md).

**Status:** accepted

## Considered Options

- **CPU plan after compile, no device** — chosen. `planBarriers()` does not require allocate. `barriers()` is a flat list. GPU Execute plays those rows on a Frame graph recorder ([ADR 0066](0066-frame-graph-gpu-execute.md)). `VkImageLayout` / access-mask mapping is the Vulkan recorder in [ADR 0067](0067-frame-graph-viewport-wire.md), not the graph.
- **Fold barriers into `compile()`** — rejected. [ADR 0061](0061-frame-graph-cpu-setup.md) forbids GPU work in compile; a barrier plan is a later phase that reads live order and accesses.
- **Record `vkCmdPipelineBarrier` in this slice** — rejected. `IGpuTexture` has no layout. `frame_graph_test` stays device-free. Command recording is GPU Execute.
- **Put the plan first thing inside `execute()`** — rejected. CPU callbacks do not need barriers. Failure would mix with `NotCompiled` / `NotAllocated`. Tests need compile → plan → read the list without running callbacks.
- **Require allocate before the plan** — rejected. The plan reads Usage and live Passes, not RHI pointers.
- **Fail CPU `execute()` when the plan is missing** — rejected for this slice. CPU callbacks did not read the plan. GPU Execute requiring a successful plan is [ADR 0066](0066-frame-graph-gpu-execute.md).
- **Image layout / Usage-only state** — rejected. WAW Clear→Forward keeps `ColorAttachment`. State is `(AccessKind, Usage)` plus Transient first-use `Undefined`. Same-Usage Read→Read is not a barrier. Same-Pass accesses are not barriers.
- **Invent Undefined before an External’s first access** — rejected. Incoming layout lives on the importer (`OffscreenRenderTarget`). Viewport wire is a later knife.
- **Texture-only plan** — rejected. Buffer uses the same rules; GPU Execute maps Buffer rows to access masks, not image layouts.
- **Lazy plan on `barriers()`** — rejected. Query stays a read. `planBarriers()` without compile returns `NotCompiled` and the list is empty.
- **Aliasing, GPU Execute, `ForwardRenderPath`** — rejected for this slice.
