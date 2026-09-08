## Why

Frame graph compile only connects Write→Read. Two ColorAttachment writers (Clear then Forward) have no edge, so DCE drops the first writer. A `/review` leftover deferred WAW/WAR until Execute; Execute still should not exist until write-order is correct.

## What Changes

- Compile grows producer/consumer edges on one handle from Setup order (Pass add order, then that Pass’s read/write call order): RAW, WAW, and WAR. Usage does not grow extra edges. Same-Pass accesses never add an edge. No Resource version type.
- Across Passes: a Read takes an edge from the previous Write; a Write takes an edge from the previous Write and from every Read after that previous Write. Read→Read has no edge.
- `read` / `write` / `markSink` on a Pass handle that was never `addPass`ed make compile return `InvalidPass` (not a silent no-op). Illegal Resource stays `DanglingAccess`.
- Compile still does not throw. When several failures apply, report one reason in this order: `InvalidPass`, `DanglingAccess`, `NoSink`, `Cycle`.
- Extend [ADR 0061](../../../docs/adr/0061-frame-graph-cpu-setup.md). Do not write ADR 0062.
- First-party `frame_graph_test` covers the five User stories. Still no Execute, GPU allocation, aliasing, barriers, or `ForwardRenderPath` replacement.

## User stories

1. I run a test where Clear and Forward both Write the same Transient as ColorAttachment and Forward is the Sink: compile keeps both Passes in that order, and the Transient’s lifetime covers both.
2. I run a test that Writes a resource, Reads it from an SSAO Pass, then Writes it again from a TAA Sink: compile keeps SSAO before TAA.
3. I run a test where A and B both Read a resource and C Writes it as the Sink: compile keeps A and B live and before C; A and B are not ordered against each other.
4. I call `read`, `write`, or `markSink` with a Pass handle that was never added: compile fails with `InvalidPass` and an empty live order. A never created or imported Resource still fails as `DanglingAccess`. If several failures apply, I get one reason: `InvalidPass`, then `DanglingAccess`, then `NoSink`, then `Cycle`.
5. I run a test where one Sink Pass Writes then Reads the same handle (or Writes it twice): compile succeeds and does not report `Cycle`.

## Capabilities

### New Capabilities

- *(none)*

### Modified Capabilities

- `frame-graph`: Compile edges include Setup-order WAW and WAR (not only RAW). Invalid Pass handles fail compile as `InvalidPass`. Failure reasons have a fixed priority. Still no Execute.

## Impact

- **Engine:** `engine/src/runtime/function/render/frame_graph/` compile and GraphBuilder invalid-Pass recording. No RHI includes. Not a Context System.
- **Tests:** extend `engine/src/tests/frame_graph_test.cpp`. Link stays `engine_runtime` only.
- **Docs:** CONTEXT Frame graph compile / GraphBuilder (Grill); ADR 0061 addendum. No ADR 0062.
- **Non-goals:** Execute / command recording; GPU allocation; memory aliasing; barriers; Resource version; replacing ForwardRenderPath; JSON; Blackboard; Frame arena; query-API sentinels on bad handles; Job System scheduling compile.
