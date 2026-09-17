## Why

Compile already orders live Passes and records Resource access. GPU Execute later needs a CPU list of layout/access transitions between those Passes. This slice records that list after compile, without `vkCmdPipelineBarrier` and without putting Vulkan into `frame_graph_test`.

## What Changes

- Add `FrameGraph::planBarriers()`. After a successful compile it records Frame graph barriers from live Pass order and Resource access. It does not require `allocate()`. It does not run Execute callbacks. It does not record GPU commands.
- Query `barriers()`: a flat ordered list. Each entry is a handle, from and to Frame graph resource state (`AccessKind` + Usage, or Transient first-use Undefined), `after` Pass (invalid on Transient first use), and `before` Pass. Order is `before` in live Pass order, then handle index. Before a successful plan, the list is empty and does not throw.
- Emit rules: skip same-Usage Read→Read across Passes and all same-Pass accesses. Emit cross-Pass RAW / WAR / WAW even when Usage is unchanged. Per `(Pass, handle)`, leaving state is that Pass’s last access of the handle; entering state is the next live Pass’s first access. Transient first use: Undefined → first live Pass entering state. External: no invented incoming state; no Undefined→… row. Texture and Buffer use the same rules.
- `planBarriers()` without a valid compile returns `NotCompiled` and leaves `barriers()` empty. Empty plan is Ok. No throw. Sticky: a second `planBarriers()` without Setup mutation is Ok and does not rebuild. Setup mutation dirties compile, allocate (existing), and the barrier plan; `barriers()` is empty until compile and `planBarriers()` succeed again.
- CPU `execute()` still does **not** require this plan. Missing plan is not a new execute failure. Existing `NotAllocated` stays allocate-only.
- New [ADR 0065](../../../docs/adr/0065-frame-graph-barrier-plan.md). [ADR 0061](../../../docs/adr/0061-frame-graph-cpu-setup.md) points here. Callbacks stay `void()`. No `vulkan.h` on the graph.

## User stories

1. I compile Clear then Forward, both Write the same Transient as ColorAttachment, then `planBarriers()` without allocate or execute: the table has a WAW row, from and to both Write+ColorAttachment, `after` is Clear, `before` is Forward.
2. I compile a live Transient Texture plus an External dummy on the same graph: Transient first use is Undefined → first Pass entering state with invalid `after`; the External has no Undefined→… row. A DCE’d Transient has no rows.
3. I Write ColorAttachment then a later live Pass Reads Sampled: RAW, from Write+Color to Read+Sampled. Two consecutive Sampled Reads: no barrier between them.
4. I compile a live Transient Buffer size 256, Write Storage then Read Storage: RAW, same rules as Texture.
5. I call `planBarriers()` with no compile: `NotCompiled`, `barriers()` empty. I compile then execute without the plan: still Ok (CPU callbacks, after allocate). I `planBarriers()` twice: the list is unchanged. I mutate Setup: `barriers()` empty; compile + `planBarriers()` again restores the table.

## Capabilities

### New Capabilities

- *(none)*

### Modified Capabilities

- `frame-graph`: `planBarriers()` after compile; `barriers()` list; emit rules (WAW, Transient first-use Undefined, no External incoming Undefined, RAW Color→Sampled, skip Read→Read, Buffer same as Texture); `NotCompiled`; sticky plan; Setup clears the plan; CPU execute does not require the plan.

## Impact

- **Engine:** `engine/src/runtime/function/render/frame_graph/` — Frame graph resource state type, `planBarriers()`, `barriers()`, Setup mutation clears the plan. No `vulkan.h`. Not a Context System.
- **Tests:** extend `engine/src/tests/frame_graph_test.cpp` for the five User stories. Existing execute paths MUST NOT be forced through `planBarriers()`. Link stays `engine_runtime` only.
- **Docs:** CONTEXT Frame graph resource state / barrier / barrier plan (Grill); ADR 0065; ADR 0061 pointer.
- **Non-goals:** `vkCmdPipelineBarrier`; aliasing; changing `setExecute`; replacing ForwardRenderPath; Vulkan allocator; JSON; Blackboard; Frame arena; Job-scheduled plan; import initial-state argument; failing CPU execute when the plan is missing.
