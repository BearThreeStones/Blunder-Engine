## Context

See proposal.md for why. Slice 1 compile only adds Write→Read edges (`frame_graph.cpp`). GraphBuilder `read` / `write` / `markSink` on a never-added Pass handle are silent no-ops. Domain: [CONTEXT.md — Frame graph compile](../../../CONTEXT.md). Decision: [ADR 0061](../../../docs/adr/0061-frame-graph-cpu-setup.md) (extend; do not add 0062).

Keep Kahn, DCE-from-Sinks-first, same-object mutate, no RHI includes, `frame_graph_test` linked to `engine_runtime` only.

## Goals / Non-Goals

**Goals:**

- Per-handle Setup-order RAW / WAW / WAR edges so ColorAttachment write chains and WAR readers survive DCE.
- Sticky `InvalidPass` and the locked failure priority.
- Tests for the five User stories; existing slice 1 cases still pass.

**Non-Goals:**

- Execute, GPU alloc, alias, barriers, Resource version, query sentinels, ForwardRenderPath, new ADR.

## Decisions

1. **Walk each resource’s accesses in Setup order, not “every writer × every reader”**  
   Keep last Write Pass and the Read Passes since that Write. On a Read, edge from last Write if it is a different Pass (RAW). On a Write, edge from last Write (WAW) and from each of those Reads (WAR), then reset the Read set and set last Write to this Pass. Skip `from == to`. Dedup edges as today.  
   *Alternatives:* consecutive access only (drops the first of two readers before a Write); RDG Resource version (rejected in Grill); treat ColorAttachment Write as a hidden Read (redundant with WAW).

2. **Sticky flag for never-added Pass handles**  
   `addAccess` / `markSink` still must not invent a Pass, but they set `m_invalid_pass` (or equivalent) so compile can see the call. Compile checks that flag before dangling Resource, NoSink, and Cycle.  
   *Alternatives:* stay silent (rejected); throw from GraphBuilder (rejected).

3. **`FrameGraphCompileReason::InvalidPass` is a new enumerator**  
   Do not reuse `DanglingAccess` for bad Pass handles. Query APIs on bad handles stay slice 1 sentinels.  
   *Alternatives:* collapse all bad handles into one reason (tests cannot tell Pass vs Resource).

4. **Failure scan order is the Grill priority**  
   `InvalidPass` → stored dangling Resource access → no Sink → build edges / DCE / Kahn → leftover live nodes are `Cycle`.  
   *Alternatives:* unspecified first-found (flaky stacked tests).

5. **Extend ADR 0061**  
   Add considered-options for Setup-order hazards vs versions, and InvalidPass vs silent drop.  
   *Alternatives:* ADR 0062 (rejected in Grill).

## Risks / Trade-offs

- [Setup-order ping-pong looks like a cycle under all-pairs RAW] → Edges only go to later Passes in Setup order. A writes X / reads Y, B writes Y / reads X, B is Sink: live order is A then B, not `Cycle`. Kahn leftover `Cycle` stays as a defensive failure.
- [Independent readers get no edge] → Kahn may emit either order; tests assert both live and before the writer, not A-then-B.
- [Invalid Pass plus dangling Resource] → Only `InvalidPass`; stacked-error tests follow priority, not a combined reason.

## Migration Plan

1. Change compile + GraphBuilder + `frame_graph_test`. Patch ADR 0061.
2. No scene, shader, or tick-path migration.
3. Rollback: restore RAW-only edges and silent invalid Pass.

## Open Questions

None.
