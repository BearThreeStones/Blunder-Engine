## Context

See proposal.md for why. `createTransient` stores a desc only (`frame_graph.cpp`). `resolvedTexture` / `resolvedBuffer` require `kind == External`. `execute()` only checks `m_compiled`. Domain: [CONTEXT.md — Frame graph allocate](../../../CONTEXT.md). CPU DAG: [ADR 0061](../../../docs/adr/0061-frame-graph-cpu-setup.md). External attach: [ADR 0063](../../../docs/adr/0063-frame-graph-resolve.md). This slice: [ADR 0064](../../../docs/adr/0064-frame-graph-allocate.md).

Keep no `vulkan.h`, same-object mutate, `frame_graph_test` linked to `engine_runtime` only, callbacks `void()`.

## Goals / Non-Goals

**Goals:**

- `allocate(IFrameGraphAllocator&)` after compile; live Transients only.
- Graph-owned `unique_ptr`; resolve raw pointer.
- Execute `NotAllocated`; allocate `NotCompiled` / `AllocFailed`; sticky allocate; Setup destroys owned Transients.
- Tests for the five User stories with a dummy allocator; existing execute cases that use live Transients call allocate first.

**Non-Goals:**

- Vulkan `createTexture(desc)`, alias, barriers, command recording, ForwardRenderPath, changing `setExecute`, storing the allocator on the graph.

## Decisions

1. **`allocate()` is its own phase**  
   `FrameGraph::allocate(IFrameGraphAllocator&)`. Compile stays CPU. Execute does not allocate.  
   *Alternatives:* Packt create in compile (ADR 0061); first thing in execute (resolve stays null; failure mixes with `NotCompiled`).

2. **Allocator is an argument, not stored**  
   Dummy in tests implements `createTexture` / `createBuffer` returning `unique_ptr`. Graph does not hold `IRenderDevice`. Sticky second call still takes the argument and does not invoke it.  
   *Alternatives:* `setAllocator` on the graph (device-facing pointer on the graph).

3. **Graph owns Transient `unique_ptr`; External stays a raw non-owning pointer**  
   Resolve returns `owned.get()` for a live allocated Transient, or the import pointer for External. Kind + shape still gate the query.  
   *Alternatives:* raw + `destroy()` (leaks); one `unique_ptr` field also wrapping External (would own an import).

4. **Failure reasons stay split**  
   Allocate without compile: `NotCompiled`, no create calls. Execute with valid compile and no allocate: `NotAllocated`. Compile invalid: execute `NotCompiled`. Null create: `AllocFailed`, destroy every Transient object from that call. No throw.  
   *Alternatives:* missing allocate as `NotCompiled` (cannot tell compile from allocate); null create as success (half-allocated Transient).

5. **Sticky until Setup mutates**  
   `noteSetupMutation` already clears compile. Also destroy owned Transient unique_ptrs; do not free External pointers.  
   *Alternatives:* realloc every `allocate()`; leave objects until the next successful allocate.

6. **ADR 0064**  
   0061 stays the CPU DAG. 0062 is Deferred. 0063 is External attach.  
   *Alternatives:* extend 0061 (hides the allocate phase); reuse 0063 (External-only).

## Risks / Trade-offs

- [Existing execute tests with live Transients] → **BREAKING**; they must `allocate(dummy)` after compile or they get `NotAllocated`.
- [Partial create then `AllocFailed`] → Rollback all Transient unique_ptrs from that call so resolve is uniformly null.
- [Sticky `allocate(allocator)` ignores the argument] → Documented; passing a different allocator on the second call does not replace objects.

## Migration Plan

1. Add allocator interface, allocate result, execute `NotAllocated`, owned Transient storage, resolve for live Transients; extend `frame_graph_test`. Keep ADR 0064 aligned.
2. No scene, shader, or tick-path migration.
3. Rollback: drop `allocate()`; execute after compile only; Transient resolve stays null.

## Open Questions

None.
