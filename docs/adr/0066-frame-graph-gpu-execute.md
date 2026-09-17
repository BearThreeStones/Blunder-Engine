# Frame graph GPU Execute records barriers onto a recorder

CPU `execute()` ran `void()` callbacks and did not read the barrier plan. We decided GPU Execute takes a Frame graph recorder each call, requires a successful `planBarriers()`, writes each live Pass’s `FrameGraphBarrier` rows through `pipelineBarrier` then runs `void(IFrameGraphRecorder&)` on that same recorder, and still does not include `vulkan.h` or replace `ForwardRenderPath`. Domain: [CONTEXT.md — Frame graph execute](../../CONTEXT.md). CPU DAG: [ADR 0061](0061-frame-graph-cpu-setup.md). Barrier plan: [ADR 0065](0065-frame-graph-barrier-plan.md).

**Status:** accepted

## Considered Options

- **`execute(IFrameGraphRecorder&)` plus `setExecute` as `void(IFrameGraphRecorder&)`** — chosen. The graph does not store the recorder. Tests pass a stand-in that logs `FrameGraphBarrier` rows. No Vulkan device.
- **Keep `void()` callbacks** — rejected. Barriers and Pass work would land on two objects; the graph could not guarantee barrier-then-callback on one recorder.
- **Expand `ICommandList`** — rejected. It only has `begin` / `end` / `submit`. This slice would bind RHI completion to the graph before viewport wire.
- **`vulkan.h` / `vkCmdPipelineBarrier` / `VkImageLayout` mapping on the graph** — rejected. Mapping belongs on a later Vulkan recorder. `frame_graph_test` stays device-free.
- **Auto `planBarriers()` inside `execute()`** — rejected. Failure would mix with `NotCompiled` / `NotAllocated`. Tests still need compile → plan → read the list → execute.
- **Missing plan as `NotAllocated`** — rejected. Allocate and plan are different phases. New reason: `NotPlanned`.
- **Skip the plan at execute** — rejected. GPU Execute’s product is playing the plan. Empty plan after a successful `planBarriers()` is still Ok.
- **`ExecuteMutated` / `CallbackFailed` when a callback dirties Setup** — rejected. Execute snapshots live order, barriers, and callbacks at entry, finishes that copy, returns `Ok`. Reasons describe entry graph state.
- **Recorder `begin` / `end` / `submit` / draw / `beginRenderPass`** — rejected. Command-buffer lifetime and draws are the caller and Pass callbacks. This slice’s recorder method is `pipelineBarrier`.
- **Wire `ForwardRenderPath`** — rejected for GPU Execute. Viewport Scene through execute is [ADR 0067](0067-frame-graph-viewport-wire.md). Overlay and copy Passes on that graph are [ADR 0068](0068-frame-graph-viewport-overlays.md).
