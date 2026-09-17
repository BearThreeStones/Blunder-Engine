## Context

See proposal.md for why. `importExternal` takes shape plus desc only (`frame_graph.h`). Compile has no `InvalidImport`. Graph headers still have no RHI types. Domain: [CONTEXT.md — Frame graph resolve](../../../CONTEXT.md). CPU DAG: [ADR 0061](../../../docs/adr/0061-frame-graph-cpu-setup.md). This slice: [ADR 0063](../../../docs/adr/0063-frame-graph-resolve.md).

Keep same-object mutate, `frame_graph_test` linked to `engine_runtime` only, callbacks `void()`, no `vulkan.h`.

## Goals / Non-Goals

**Goals:**

- Import attaches a non-owning `rhi::IGpuTexture*` or `rhi::IGpuBuffer*`.
- Setup `resolvedTexture` / `resolvedBuffer` without compile.
- Compile `InvalidImport` with the locked reason priority.
- Tests for the five User stories using dummy RHI stand-ins; existing import call sites take a dummy pointer.

**Non-Goals:**

- Transient GPU allocation, alias, barriers, command recording, ForwardRenderPath, `VkImage` on the graph, changing `setExecute`, wrapping `OffscreenRenderTarget`.

## Decisions

1. **Two import overloads; shape follows the pointer**  
   `importExternal(desc, IGpuTexture*, name)` and `importExternal(desc, IGpuBuffer*, name)`. `createTransient` still takes shape. No separate `bind()`.  
   *Alternatives:* keep a shape argument (duplicate); `bind()` after import (two attach paths).

2. **Resolved type is RHI, not Vulkan, not `void*`**  
   Forward-declare `rhi::IGpuTexture` and `rhi::IGpuBuffer` on the graph header. Tests construct dummy subclasses (`IGpuTexture` is empty; `IGpuBuffer` stubs `upload` / `size`). Viewport `VkImage` stays off this slice.  
   *Alternatives:* store `VkImage` (forces a device and a Vulkan include); `void*` (Texture handle could attach anything).

3. **Null import is `InvalidImport` at compile**  
   Setup accepts nullptr and does not throw. Unused null still fails. Priority: `InvalidPass` → `InvalidDesc` → `InvalidImport` → `DanglingAccess` → `NoSink` → `Cycle`.  
   *Alternatives:* throw at import (rejected: compile is the error channel); fold into `InvalidDesc` (wrong: desc fields may be legal); treat null as legal resolve-null (External would be half-imported).

4. **Resolve is Setup state**  
   Same query pattern as `resourceDesc`. DCE does not erase. Miss cases return nullptr. Transient has no object this slice.  
   *Alternatives:* only inside `execute()` callbacks (would force a callback-signature change this slice).

5. **ADR 0063, not 0061 and not 0062**  
   0061 stays the CPU DAG. 0062 is Deferred Render Path.  
   *Alternatives:* extend 0061 (hides the RHI layering switch); reuse 0062 (number taken).

## Risks / Trade-offs

- [Existing `importExternal(shape, desc, name)` call sites] → **BREAKING**; update `frame_graph_test` with dummy pointers.
- [Viewport color is `VkImage` on `OffscreenRenderTarget`] → later wire-viewport wraps or adapts; this slice does not.
- [Caller frees the RHI object while the graph still holds the pointer] → non-owning; caller lifetime. Graph does not uniquify two imports of the same pointer.

## Migration Plan

1. Add `InvalidImport`, import overloads, stored pointers, `resolvedTexture` / `resolvedBuffer`; extend `frame_graph_test`. Write ADR 0063; point ADR 0061 at it.
2. No scene, shader, or tick-path migration.
3. Rollback: restore shape+desc import without pointers; CPU desc graph remains from the previous slice.

## Open Questions

None.
