## 1. Barrier plan API

- [x] 1.1 Add `FrameGraphResourceState` (Undefined plus AccessKind and Usage; not `FrameGraphFormat::Undefined`), `FrameGraphBarrier` (handle, from, to, `after`, `before`), `FrameGraphPlanBarriersReason` (`Ok`, `NotCompiled`), `planBarriers()`, and `barriers()`. No `vulkan.h`.
- [x] 1.2 Implement emit rules: live resources only; Transient first-use Undefined with invalid `after`; no External incoming Undefined; skip same-Usage Read→Read and same-Pass accesses; emit cross-Pass RAW / WAR / WAW even when Usage is unchanged; leaving = last access on that Pass; entering = first access on the next accessing live Pass; Texture and Buffer share rules; sort by `before` live order then handle index.
- [x] 1.3 Failures and sticky: no compile → `NotCompiled` and empty `barriers()`. Empty plan is Ok. Second `planBarriers()` without Setup mutation does not rebuild. `noteSetupMutation` and compile `fail()` clear the plan. Successful `compile()` does not drop a sticky plan. `planBarriers()` does not require allocate. CPU `execute()` does not require the plan.

## 2. Tests

- [x] 2.1 Extend `frame_graph_test` (no Vulkan device). Do not force existing execute paths through `planBarriers()`. Cover the five User stories: Clear→Forward ColorAttachment WAW without allocate/execute; Transient first-use Undefined + External has no Undefined row + DCE’d Transient has no rows; RAW Color→Sampled and skip Read→Read; Buffer 256 Write Storage then Read Storage; `NotCompiled` / execute without plan still Ok / sticky list / Setup clears then compile+plan restores.
- [x] 2.2 Build and run `frame_graph_test` (`build/vs2026-debug`, Debug).

## 3. ADR

- [x] 3.1 Keep [ADR 0065](../../../docs/adr/0065-frame-graph-barrier-plan.md) and the [ADR 0061](../../../docs/adr/0061-frame-graph-cpu-setup.md) pointer aligned with the shipped barrier-plan API.
