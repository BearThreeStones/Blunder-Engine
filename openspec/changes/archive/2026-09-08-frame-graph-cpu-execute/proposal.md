## Why

Slice 2 compile now keeps write-order live, but nothing runs that order. Packt records GPU work inside `compile()`; we still need a CPU Execute that proves live-order callbacks before GPU alloc, barriers, or `ForwardRenderPath` replacement.

## What Changes

- After a successful compile, `FrameGraph::execute()` runs each live Pass’s CPU callback in live order. Callbacks are `void()`, recorded with GraphBuilder `setExecute`. DCE’d Passes do not run. A live Pass with no callback is an empty run.
- `execute()` returns `{ok, reason}` and does not throw. It does not compile. If the last compile was not `Ok`, or Setup changed since that compile, execute returns `NotCompiled` and runs nothing. It does not echo compile failure reasons (`InvalidPass`, `NoSink`, …).
- `setExecute` on a Pass handle that was never added is sticky `InvalidPass` at compile, same as `read` / `write` / `markSink`.
- Any Setup mutation (`create` / `import` / `addPass` / `read` / `write` / `markSink` / `setExecute`) dirties the last compile. The same successful compile may be executed more than once. Callbacks must not throw; execute does not catch.
- Extend [ADR 0061](../../../docs/adr/0061-frame-graph-cpu-setup.md). Do not write ADR 0062.
- First-party `frame_graph_test` covers the five User stories. Still no handle→RHI resolve, GPU allocation, aliasing, barriers, or `ForwardRenderPath` replacement.

## User stories

1. I run a test with Clear then Forward (Sink) plus a Pass that DCE drops; all three have `setExecute`: `execute()` succeeds and runs Clear then Forward only; the culled callback never runs.
2. I leave a live Sink without a callback: `execute()` still succeeds as an empty run; live Passes that have callbacks still run.
3. I call `execute()` when compile has never succeeded, or the last compile failed: execute returns `NotCompiled`, runs no callbacks, does not compile, and does not echo `InvalidPass` / `NoSink` / other compile reasons.
4. After a successful compile I mutate Setup (including `setExecute`) and execute without compiling again: `NotCompiled`, no callbacks. After compile succeeds again, execute runs; the same successful compile may execute twice and each callback runs twice.
5. I call `setExecute` on a Pass handle that was never added: compile fails with `InvalidPass` and an empty live order.

## Capabilities

### New Capabilities

- *(none)*

### Modified Capabilities

- `frame-graph`: GraphBuilder records `setExecute`; compile success plus a clean Setup allows CPU Execute of live Pass callbacks; `NotCompiled` when uncompiled or dirty; still no RHI.

## Impact

- **Engine:** `engine/src/runtime/function/render/frame_graph/` GraphBuilder `setExecute`, `FrameGraph::execute()`, dirty-compile flag. No RHI includes. Not a Context System.
- **Tests:** extend `engine/src/tests/frame_graph_test.cpp`. Link stays `engine_runtime` only.
- **Docs:** CONTEXT Frame graph execute / GraphBuilder (Grill); ADR 0061 addendum. No ADR 0062.
- **Non-goals:** resolving handles to RHI; GPU allocation; memory aliasing; barriers; command recording; replacing ForwardRenderPath; JSON; Blackboard; Frame arena; query-API sentinels; Job System scheduling Execute; catching callback exceptions; ADR 0062.
