## Why

CPU `execute()` runs `void()` callbacks and does not read the barrier plan. GPU Execute needs those planned rows written onto a recorder before each live Pass, still without a Vulkan device in `frame_graph_test` and without replacing `ForwardRenderPath`.

## What Changes

- **BREAKING:** `GraphBuilder::setExecute` takes `void(IFrameGraphRecorder&)`. `FrameGraph::execute(IFrameGraphRecorder&)` takes a recorder each call; the graph does not store it.
- **BREAKING:** `execute()` requires a successful `planBarriers()` after that compile. Missing plan is `NotPlanned`, not `NotAllocated`. Empty plan after a successful `planBarriers()` is Ok. Execute does not call `planBarriers()` internally.
- For each live Pass, execute writes every `barriers()` row whose `before` is that Pass via `IFrameGraphRecorder::pipelineBarrier(const FrameGraphBarrier&)`, then runs that Pass’s callback with the **same** recorder. A live Pass with no callback is an empty run after its barriers. DCE’d Passes do not run.
- Recorder this slice: only `pipelineBarrier`. No `VkImageLayout` mapping, no `vkCmdPipelineBarrier`, no draw / `beginRenderPass` / `begin` / `end` / `submit`. No `vulkan.h` on the graph. Not `ICommandList`.
- Failure order: `NotCompiled`, then `NotAllocated`, then `NotPlanned`. Any failure records nothing. Execute snapshots live Pass order, the barrier list, and callbacks at entry so a forbidden Setup mutation does not UAF; still returns `Ok`. No `ExecuteMutated` / `CallbackFailed`.
- New [ADR 0066](../../../docs/adr/0066-frame-graph-gpu-execute.md). [ADR 0061](../../../docs/adr/0061-frame-graph-cpu-setup.md) and [ADR 0065](../../../docs/adr/0065-frame-graph-barrier-plan.md) point here.

## User stories

1. I compile Clear then Forward, both Write the same Transient as ColorAttachment, allocate and `planBarriers()`, then `execute(dummy)`: Dummy gets the first-use row, then the Clear callback, then the WAW row, then the Forward callback. The Forward callback’s recorder is the same Dummy I passed to `execute()`.
2. I compile and allocate, skip `planBarriers()`, then `execute(dummy)`: `NotPlanned`. Dummy has no rows. No callback runs.
3. I have only an External Sink Write (empty plan): `planBarriers()` Ok, `execute(dummy)` Ok. Dummy calls `pipelineBarrier` zero times. The Sink callback runs once.
4. I Write ColorAttachment then a later live Pass Reads Sampled: the RAW row is recorded before the reader callback. Two consecutive Sampled Reads: no barrier between them. A Transient Buffer Storage Write then Read: RAW before the reader, same rules as Texture.
5. I `execute(dummy)` with no compile: `NotCompiled`. I compile and skip allocate (and skip plan): `NotAllocated`. After compile + allocate + plan, a second `execute(dummy)` without Setup mutation records the same sequence. After Setup mutation, execute is `NotCompiled` and records nothing.

## Capabilities

### New Capabilities

- *(none)*

### Modified Capabilities

- `frame-graph`: GPU Execute on a Frame graph recorder; `setExecute` as `void(IFrameGraphRecorder&)`; require `planBarriers()` (`NotPlanned`); play `barriers()` before each live Pass; empty plan Ok; failure order; snapshot; tests use a Dummy recorder; no Vulkan device.

## Impact

- **Engine:** `engine/src/runtime/function/render/frame_graph/` — `IFrameGraphRecorder`, `execute(IFrameGraphRecorder&)`, `NotPlanned`, `setExecute` signature. No `vulkan.h`. Not a Context System.
- **Tests:** extend `engine/src/tests/frame_graph_test.cpp` for the five User stories. Existing execute paths MUST `planBarriers()` and pass a Dummy recorder; existing `setExecute` lambdas take the recorder argument. Link stays `engine_runtime` only.
- **Docs:** CONTEXT Frame graph execute / recorder (Grill); ADR 0066; ADR 0061 / 0065 pointers.
- **Non-goals:** `ForwardRenderPath`; `VkImageLayout` mapping; expanding `ICommandList`; recorder draw / begin-end-submit; Vulkan allocator; aliasing; JSON; Blackboard; Frame arena; Job-scheduled execute.
