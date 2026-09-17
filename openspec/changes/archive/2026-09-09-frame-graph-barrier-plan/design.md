## Context

See proposal.md for why. `compile()` builds live Pass order and per-Pass `Access` (`kind` + `usage`). `allocate()` is independent. `execute()` runs `void()` callbacks and does not read a barrier list. Domain: [CONTEXT.md — Frame graph barrier plan](../../../CONTEXT.md). CPU DAG: [ADR 0061](../../../docs/adr/0061-frame-graph-cpu-setup.md). Allocate: [ADR 0064](../../../docs/adr/0064-frame-graph-allocate.md). This slice: [ADR 0065](../../../docs/adr/0065-frame-graph-barrier-plan.md).

Keep no `vulkan.h`, same-object mutate, `frame_graph_test` linked to `engine_runtime` only, callbacks `void()`.

## Goals / Non-Goals

**Goals:**

- `planBarriers()` after compile; no allocate required.
- `barriers()` flat list; Frame graph resource state is `(AccessKind, Usage)` plus Transient first-use Undefined (not `FrameGraphFormat::Undefined`).
- Emit / skip rules from Grill; Texture and Buffer share them.
- `NotCompiled` only; sticky until Setup; Setup clears the list; CPU execute does not require the plan.
- Tests for the five User stories; existing execute paths are not forced through `planBarriers()`.

**Non-Goals:**

- `vkCmdPipelineBarrier`, aliasing, ForwardRenderPath, changing `setExecute`, import initial-state argument, Job-scheduled plan, failing CPU execute when the plan is missing.

## Decisions

1. **`planBarriers()` is its own phase**  
   `FrameGraph::planBarriers()` takes no allocator and no extra argument. Compile stays CPU DAG. Execute does not plan.  
   *Alternatives:* fold into `compile()` (ADR 0061); first thing in `execute()` (CPU callbacks do not need it; failure mixes with `NotCompiled` / `NotAllocated`); lazy on `barriers()` (query would compile a plan).

2. **State is a small struct, not image layout**  
   `FrameGraphResourceState`: Undefined flag plus `access` and `usage` when defined. Do not reuse `FrameGraphFormat::Undefined`. `FrameGraphBarrier`: handle, from, to, `after`, `before`. `after` is invalid on Transient first use.  
   *Alternatives:* `VkImageLayout`; Usage-only (cannot express Clear→Forward WAW); Texture-only (Grill: Buffer same rules).

3. **Walk live accesses per handle**  
   For each live resource, walk live Passes that access that handle in live Pass order. Leaving state = that Pass’s last access of the handle (Setup call order). Entering state = the next such Pass’s first access. Transient: emit Undefined → first entering state with invalid `after`. External: do not emit that row. Between consecutive accessing Passes: skip only same-Usage Read→Read; emit RAW / WAR / WAW and Usage changes. Same-Pass accesses never emit. DCE’d resources emit nothing. Sort: `before` in live Pass order, then handle index.  
   *Alternatives:* invent External incoming Undefined; intra-pass barriers; skip WAW when Usage is unchanged.

4. **Failure and sticky match allocate’s compile gate, not execute**  
   Reasons: `Ok`, `NotCompiled`. Empty list is Ok. Sticky second call does not rebuild. `noteSetupMutation` and compile `fail()` clear the list and the planned flag. Successful `compile()` does not drop a sticky plan. `barriers()` is empty unless compile is valid and the plan flag is set. CPU `execute()` does not read the flag.  
   *Alternatives:* require allocate; fail execute when missing; rebuild every `planBarriers()`; clear the plan on every `compile()`.

5. **ADR 0065**  
   0061 stays the CPU DAG. 0064 stays allocate.  
   *Alternatives:* extend 0061 (hides the phase); reuse 0064 (allocate-only).

## Risks / Trade-offs

- [Clear→Forward WAW also has a Transient first-use row] → Story 1 asserts the WAW row exists; story 2 asserts first-use. Tests must not treat the WAW row as the only row.
- [Existing execute tests] → Do not add `planBarriers()` to those paths. Missing plan is not `NotAllocated`.
- [Sticky `planBarriers()` after a second successful compile without Setup] → Same live order; skip rebuild. Match allocate.

## Migration Plan

1. Add state / barrier types, `planBarriers()`, `barriers()`, clear on Setup and compile fail; extend `frame_graph_test`. Keep ADR 0065 aligned.
2. No scene, shader, or tick-path migration.
3. Rollback: drop `planBarriers()` / `barriers()`; execute and allocate unchanged.

## Open Questions

None.
