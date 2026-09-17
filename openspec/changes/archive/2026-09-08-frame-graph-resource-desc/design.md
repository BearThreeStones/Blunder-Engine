## Context

See proposal.md for why. GraphBuilder `createTransient` / `importExternal` take shape plus optional debug name only (`frame_graph.cpp`). Compile has no `InvalidDesc`. Domain: [CONTEXT.md — Frame graph resource desc](../../../CONTEXT.md). Decision: [ADR 0061](../../../docs/adr/0061-frame-graph-cpu-setup.md) (extend; do not add 0062).

Keep no RHI includes, same-object mutate, `frame_graph_test` linked to `engine_runtime` only.

## Goals / Non-Goals

**Goals:**

- Required CPU Frame graph resource desc on create and import (same type).
- `resourceDesc` query without compile; DCE does not erase the desc.
- Compile `InvalidDesc` with the locked field rules and reason priority.
- Tests for the five User stories; existing cases pass a legal desc.

**Non-Goals:**

- Handle→RHI resolve, GPU alloc, alias, barriers, command recording, ForwardRenderPath, device object on import, expanding `rhi::PixelFormat`, new ADR.

## Decisions

1. **One `FrameGraphResourceDesc` for Texture and Buffer**  
   Shape stays the create/import argument. Texture uses format / width / height / samples / mips; Buffer uses size. Unused fields are ignored for the other shape.  
   *Alternatives:* two desc types (Grill: same type); put shape inside the desc (rejected: would duplicate the existing argument).

2. **`FrameGraphFormat` on the graph header**  
   `Undefined`, `R8G8B8A8_UNORM`, `D32_SFLOAT`. Not `VkFormat`, not `rhi::PixelFormat`, no RHI include. Buffer has no format.  
   *Alternatives:* store `VkFormat` (breaks no-device tests); extend `rhi::PixelFormat` in this slice (rejected in Grill).

3. **Create/import accept illegal desc; compile reports `InvalidDesc`**  
   Do not throw at Setup. An unused illegal resource still fails compile. Buffer ignores format; Texture ignores size. samples/mips must be ≥ 1; this slice does not require 1/2/4/8.  
   *Alternatives:* reject at create (rejected: compile is the error channel); only live resources (rejected: Setup error).

4. **Reason priority inserts `InvalidDesc` after `InvalidPass`**  
   `InvalidPass` → `InvalidDesc` → `DanglingAccess` → `NoSink` → `Cycle`. Execute after that compile is `NotCompiled`.  
   *Alternatives:* reuse `DanglingAccess` (wrong: the resource exists); fail only unused-legal DCE (rejected).

5. **`resourceDesc` is Setup state**  
   Same query pattern as `resourceKind`: no compile required; never-created handle returns a default-constructed desc. Extent is `uint32` width/height on the desc, not `rhi::Extent2D`.  
   *Alternatives:* require compile first (rejected in Grill).

6. **Extend ADR 0061**  
   Add considered-options for CPU desc vs RHI on Setup, format enum vs `VkFormat`, `InvalidDesc` vs `DanglingAccess`.  
   *Alternatives:* ADR 0062 (rejected in Grill).

## Risks / Trade-offs

- [Existing `createTransient(shape, name)` call sites] → **BREAKING** signature; update `frame_graph_test` with a legal desc helper.
- [Default-constructed desc is all-zero / `Undefined`] → That is `InvalidDesc` at compile; tests that want success must fill fields.
- [MSAA sample counts other than 1] → Allowed if ≥ 1; GPU alloc later maps what the device supports.

## Migration Plan

1. Add desc types + `resourceDesc` + `InvalidDesc`; update GraphBuilder create/import; extend `frame_graph_test`. Patch ADR 0061.
2. No scene, shader, or tick-path migration.
3. Rollback: restore name-only create/import; compile-only graph without desc remains from prior slices.

## Open Questions

None.
