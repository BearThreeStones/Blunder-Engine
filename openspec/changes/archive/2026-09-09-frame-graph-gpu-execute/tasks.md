## 1. GPU Execute API

- [x] 1.1 Add `IFrameGraphRecorder` with `pipelineBarrier(const FrameGraphBarrier&)`, `FrameGraphExecuteReason::NotPlanned`, `execute(IFrameGraphRecorder&)`, and `setExecute` as `void(IFrameGraphRecorder&)`. Graph does not store the recorder. No `vulkan.h`. Not `ICommandList`.
- [x] 1.2 Implement execute: require compile, then allocate, then a successful plan (`NotCompiled` / `NotAllocated` / `NotPlanned` in that order). Empty plan is Ok. Snapshot live Pass order, barrier list, and callbacks. For each snapshotted live Pass, call `pipelineBarrier` for rows whose `before` is that Pass (plan order), then run the callback with the same recorder. Do not `planBarriers()` inside execute. Do not begin/end/submit.
- [x] 1.3 Update every existing `frame_graph_test` `setExecute` / `execute()` call site: lambdas take the recorder; successful execute paths call `planBarriers()` and pass a Dummy recorder.

## 2. Tests

- [x] 2.1 Extend `frame_graph_test` (no Vulkan device). Cover the five User stories: Clear→Forward first-use then WAW around callbacks plus same Dummy pointer; compile+allocate without plan → `NotPlanned` and empty Dummy; External-only empty plan → zero `pipelineBarrier` and Sink callback once; RAW before Sampled reader, skip Read→Read, Buffer Storage RAW before reader; `NotCompiled` / `NotAllocated` beats `NotPlanned` / sticky second execute / Setup dirty → `NotCompiled`.
- [x] 2.2 Build and run `frame_graph_test` (`build/vs2026-debug`, Debug).

## 3. ADR

- [x] 3.1 Keep [ADR 0066](../../../docs/adr/0066-frame-graph-gpu-execute.md) and the [ADR 0061](../../../docs/adr/0061-frame-graph-cpu-setup.md) / [ADR 0065](../../../docs/adr/0065-frame-graph-barrier-plan.md) pointers aligned with the shipped execute API.
