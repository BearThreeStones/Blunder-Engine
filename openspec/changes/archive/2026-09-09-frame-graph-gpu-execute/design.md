## Context

See proposal.md for why. `setExecute` is `void()`. `execute()` runs CPU callbacks, does not read `barriers()`, and does not take a recorder. Domain: [CONTEXT.md — Frame graph execute](../../../CONTEXT.md). CPU DAG: [ADR 0061](../../../docs/adr/0061-frame-graph-cpu-setup.md). Barrier plan: [ADR 0065](../../../docs/adr/0065-frame-graph-barrier-plan.md). This slice: [ADR 0066](../../../docs/adr/0066-frame-graph-gpu-execute.md).

Keep no `vulkan.h` on the graph, same-object mutate, `frame_graph_test` linked to `engine_runtime` only.

## Goals / Non-Goals

**Goals:**

- `IFrameGraphRecorder` with `pipelineBarrier(const FrameGraphBarrier&)`.
- `execute(IFrameGraphRecorder&)`; `setExecute` as `void(IFrameGraphRecorder&)`.
- Require successful `planBarriers()`; `NotPlanned`; empty plan Ok; play rows whose `before` is the current live Pass, then the callback.
- Failure order `NotCompiled`, `NotAllocated`, `NotPlanned`. Snapshot live order, barriers, callbacks. Tests with Dummy recorder; no Vulkan device.

**Non-Goals:**

- `ForwardRenderPath`, `VkImageLayout` mapping, expanding `ICommandList`, recorder draw / begin-end-submit, Vulkan allocator, aliasing, storing the recorder, auto-plan inside execute.

## Decisions

1. **Recorder is an argument, like the allocator**  
   `execute(IFrameGraphRecorder&)`. Graph does not store it.  
   *Alternatives:* expand `ICommandList` (only `begin` / `end` / `submit`); store the recorder (device-facing pointer on the graph).

2. **Same recorder for barriers and callbacks**  
   `setExecute` is `void(IFrameGraphRecorder&)`. Graph writes that Pass’s rows, then calls the callback with the same object.  
   *Alternatives:* keep `void()` (two objects; graph cannot guarantee order).

3. **Play the CPU plan as `FrameGraphBarrier` rows**  
   No layout mapping this slice. Dummy logs the structs.  
   *Alternatives:* `vkCmdPipelineBarrier` / `vulkan.h` on the graph (breaks device-free tests); map to `VkImageLayout` here (belongs on a later Vulkan recorder).

4. **Execute requires the plan; it does not plan**  
   Reasons: `Ok`, `NotCompiled`, `NotAllocated`, `NotPlanned`. Empty list after a successful `planBarriers()` is Ok.  
   *Alternatives:* missing plan still Ok (ADR 0065 CPU execute); treat missing plan as `NotAllocated`; auto `planBarriers()` inside execute.

5. **Snapshot at entry, still `Ok`**  
   Copy live Pass order, barrier list, and callbacks. Forbidden Setup mutation must not UAF. No `ExecuteMutated`.  
   *Alternatives:* abort remaining Passes; new failure reason (mixes entry graph state with callback misconduct).

6. **ADR 0066**  
   0061 stays the CPU DAG. 0065 stays the CPU plan.  
   *Alternatives:* extend 0061 (hides GPU Execute); extend 0065 (hides the recorder).

## Risks / Trade-offs

- [Existing execute tests] → **BREAKING**; every `setExecute` lambda takes the recorder; every `execute()` needs `planBarriers()` and a Dummy recorder.
- [Clear→Forward Dummy log] → first-use row (`before` = Clear) then Clear callback then WAW (`before` = Forward) then Forward callback. Tests must not treat the WAW row as the only row.
- [Sticky second `execute(recorder)`] → Same copied plan; passing a different Dummy on the second call still gets the same rows.

## Migration Plan

1. Add recorder interface, `NotPlanned`, `execute(IFrameGraphRecorder&)`, `setExecute` signature, play barriers then callbacks, snapshot copies; extend `frame_graph_test`. Keep ADR 0066 aligned.
2. No scene, shader, or tick-path migration.
3. Rollback: restore `void()` / no-arg `execute()`; drop `NotPlanned`; execute does not read the plan.

## Open Questions

None.
