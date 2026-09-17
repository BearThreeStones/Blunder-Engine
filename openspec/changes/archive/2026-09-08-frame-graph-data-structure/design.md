## Context

See proposal.md for why. Grill locked a CPU Frame graph (virtual handles, Transient vs External, GraphBuilder, compile = edges + topo + DCE + lifetimes, no Blackboard, no Frame arena, compile returns failure without throw). Viewport work stays immediate-mode in `ForwardRenderPath`. Packt chapter4 embeds `TextureHandle` on Setup and creates Vulkan images in `compile()` — out of scope. Domain: [CONTEXT.md — Rendering](../../../CONTEXT.md). Decision: [ADR 0061](../../../docs/adr/0061-frame-graph-cpu-setup.md).

Render sources are listed in `function/render/CMakeLists.txt`. Tests that must not pull Vulkan follow `job_system_test` (`engine_runtime` only).

## Goals / Non-Goals

**Goals:**

- Caller-owned Frame graph + GraphBuilder types tests can construct without a device.
- Compile writes live Pass order and Resource lifetimes onto that object, or returns failure with an empty order.
- `frame_graph_test` covering the five User stories.

**Non-Goals:**

- Mounting a Context System or touching `RuntimeGlobalContext`.
- Per-tick reset, Frame arena, JSON, Blackboard, Execute callbacks.
- Linking `frame_graph` headers to Vulkan / RHI.

## Decisions

1. **Sources live under `function/render/frame_graph/`**  
   This is render scheduling data, not Privileged core and not a Context System.  
   *Alternatives:* `function/job/` (Job System is CPU workers, not GPU Pass order); `core/` (kernel).

2. **No RHI or Vulkan includes on graph headers**  
   Setup identity is a Frame graph handle (dense index). GPU objects stay on later Execute.  
   *Alternatives:* Packt `TextureHandle` on the resource (rejected in Grill).

3. **Compile mutates the same Frame graph**  
   Success: live Pass sequence + lifetimes. Failure: `ok == false`, empty live order. No second `CompiledFrameGraph` type.  
   *Alternatives:* immutable compile output object (extra type for v1).

4. **Compile result is a struct, not an exception**  
   `ok` plus a reason enum (cycle, dangling access, no sink). Tests assert `ok` and reason.  
   *Alternatives:* throw; `std::expected` as the only API (optional later, not required).

5. **Resource identity is the handle, not a string**  
   Optional debug name for tests. Edges follow handle equality.  
   *Alternatives:* Packt name-keyed `get_resource` as the only identity (stringly typed).

6. **Topo is Kahn (or equivalent) on live producer→consumer Pass edges**  
   DCE from Sinks runs first. Cycle detection is a leftover-node check after Kahn on the live subgraph only. Unreachable cycles are dropped, not a compile failure.  
   *Alternatives:* DFS with color marks (also fine; Kahn matches “empty order on cycle”).

7. **DCE walks backward from Sink-marked Passes**  
   A Pass is live if a Sink can reach it through resource edges (consumer to producer).  
   *Alternatives:* treat every External as a Sink (rejected in Grill).

8. **Tests construct the graph directly**  
   `frame_graph_test` does not boot `RenderSystem` or `startSystems`.  
   *Alternatives:* tick-path integration (out of v1).

9. **Ordinary `std::vector` storage**  
   No Frame arena. Caller owns the Frame graph for the test lifetime.  
   *Alternatives:* Filament linear allocator (rejected for this slice).

## Risks / Trade-offs

- [Later Execute wants GPU handles on the same structs] → Keep a separate resolve table; do not put `VkImage` on Setup types in a “quick” follow-up.
- [Name-only graphs from Packt samples] → Builder is handle-based; do not add JSON to make samples drop in.
- [Compile on the tick later allocates] → v1 containers are enough; Frame arena is a later change, not a silent add in this PR.
- [DCE vs disabled Passes] → v1 has no enable flag; omit Passes by not adding them. Add enable in a later slice if tick rebuild needs it.

## Migration Plan

1. Land types + `frame_graph_test` + CMake. Write ADR 0061 if not already in tree.
2. No scene or shader migration. Forward path unchanged.
3. Rollback: delete the library and test; no on-disk format.

## Open Questions

None.
