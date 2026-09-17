## 1. Execute API and dirty compile

- [x] 1.1 Add `FrameGraphExecuteReason::{Ok, NotCompiled}` and `FrameGraphExecuteResult`. `FrameGraph::execute()` returns that result, does not throw, does not compile, does not catch callbacks.
- [x] 1.2 `GraphBuilder::setExecute(pass, std::function<void()>)` last-wins on the Pass. Never-added Pass handle sets sticky `InvalidPass` (do not invent a Pass, do not throw).
- [x] 1.3 Every Setup mutation (`create` / `import` / `addPass` / `read` / `write` / `markSink` / `setExecute`) clears compile output and the compiled flag. `compile()` Ok sets the flag. Execute with the flag off returns `NotCompiled` and runs nothing.

## 2. Tests

- [x] 2.1 Extend `frame_graph_test`: Clear→Forward Sink plus DCE’d Pass (live callbacks in order, culled never runs); live Sink with no callback (empty run, others still run); execute before compile and after failed compile (`NotCompiled`, no callbacks, reason is not a compile reason); dirty Setup then execute (`NotCompiled`); recompile after dirty then execute; same compile execute twice; `setExecute` on never-added handle → compile `InvalidPass`. Existing compile cases still pass.
- [x] 2.2 Build and run `frame_graph_test` (`build/vs2026-debug`, Debug).

## 3. ADR

- [x] 3.1 Extend [ADR 0061](../../../docs/adr/0061-frame-graph-cpu-setup.md): CPU `void()` callbacks in live order; dirty compile; `NotCompiled`; must-not-throw / no catch. Do not add ADR 0062.
