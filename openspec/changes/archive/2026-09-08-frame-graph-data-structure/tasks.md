## 1. Types and GraphBuilder

- [x] 1.1 Add `engine/src/runtime/function/render/frame_graph/frame_graph.h` and `frame_graph.cpp`: caller-owned Frame graph, Frame graph handle (dense index), Transient vs External, Texture vs Buffer, Resource access (Read/Write + usage), Pass with Sink mark. No Vulkan/RHI includes. No Blackboard. Ordinary containers (no Frame arena).
- [x] 1.2 Implement GraphBuilder on that type: create Transient, import External, add Pass read/write, mark Sink. Optional debug names; identity is the handle.
- [x] 1.3 Wire the sources into `engine/src/runtime/function/render/CMakeLists.txt`.

## 2. Compile

- [x] 2.1 Implement Frame graph compile: producer/consumer edges, Kahn (or equivalent) topo, DCE backward from Sinks, Resource lifetime `[first, last]` on live Resources. Mutate the same object. Return a result struct (`ok` + reason). Do not throw. Do not allocate GPU memory.
- [x] 2.2 Cycle, dangling create/import miss, and zero Sinks: `ok == false` and empty live Pass order. Unreachable Passes are dropped, not a failure.

## 3. Tests

- [x] 3.1 Add `engine/src/tests/frame_graph_test.cpp`: two-Pass Transient then Sink (order + lifetime); unreachable post-process dropped; imported External stays External; Buffer write then read; cycle / dangling access / zero Sinks fail without throw.
- [x] 3.2 Wire `frame_graph_test` in `engine/src/tests/CMakeLists.txt` (link `engine_runtime` only; no Vulkan / Slint).
- [x] 3.3 Build and run `frame_graph_test` (`build/vs2026-debug`, Debug).

## 4. Docs

- [x] 4.1 Write [ADR 0061](../../../docs/adr/0061-frame-graph-cpu-setup.md) if missing. Link it from CONTEXT Frame graph. Do not expand v1 into JSON, Blackboard, arena, Execute, or ForwardRenderPath replacement.
