## Why

Viewport work is still an immediate-mode sequence in `RenderSystem::tick` (`ForwardRenderPath`, overlay, SSAO). Explicit Vulkan needs a retained-mode Frame graph before aliasing, barriers, or Execute. This slice lands the CPU graph so later slices can compile against a tested DAG instead of Packt-style GPU handles on Setup.

## What Changes

- Add a caller-owned **Frame graph**: Passes, named Resources (Texture or Buffer, Transient or External), producer/consumer edges from Resource access (Read/Write + usage).
- **GraphBuilder** records Setup: create, import, read, write, Sink marks. No JSON, no Blackboard, no Frame arena.
- **Frame graph compile** builds edges, topological order, DCE from Sinks, and Resource lifetimes. It does not allocate GPU memory, alias, insert barriers, or record commands. It does not throw: cycle, access to a never created/imported Resource, or zero Sinks returns failure and an empty live order.
- First-party `frame_graph_test` is the v1 caller. Do not replace `ForwardRenderPath` or wire `RenderSystem::tick`.

## User stories

1. I run a test that builds “write a Transient, then sample it from a Sink” with GraphBuilder: compile yields that order, and the Transient’s lifetime covers both Passes.
2. I add a post-process Pass that no Sink uses: compile drops that Pass and its Transient; the Sink subgraph stays.
3. I import an External stand-in for viewport color and write it from a Sink Pass: compile keeps it as External and does not treat it as a Transient the graph would allocate.
4. I compile a cycle, a read/write of a Resource that was never created or imported, or a graph with no Sink: compile returns failure, does not throw, and the live Pass order is empty.
5. I create a Buffer Transient, write it, then read it: compile tracks edges and lifetime the same way as a Texture (still no GPU allocation).

## Capabilities

### New Capabilities

- `frame-graph`: CPU Frame graph Setup via GraphBuilder, compile (edges, topo, DCE from Sinks, Resource lifetimes), Transient vs External, Texture vs Buffer, compile failure without exceptions.

### Modified Capabilities

- *(none — ForwardRenderPath, Secondary command buffers, Bindless, Texture Loader, Job System, and viewport present stay as they are.)*

## Impact

- **Engine:** new types under `engine/src/runtime/function/render/frame_graph/`; wire into `function/render/CMakeLists.txt`. Not a Context System. No RHI includes on the graph types.
- **Tests:** `engine/src/tests/frame_graph_test.cpp` linked like `job_system_test` (no Vulkan / Slint).
- **Docs:** CONTEXT Rendering terms from Grill; [ADR 0061](../../../docs/adr/0061-frame-graph-cpu-setup.md).
- **Non-goals:** JSON `graph.json`; Blackboard; Frame arena; memory aliasing; barriers; Execute / command recording; replacing ForwardRenderPath; GPU pick / Texture Loader / Mesh Preview / Camera Preview as graph Passes; Job System scheduling compile; Bindless slots as graph Resources.
