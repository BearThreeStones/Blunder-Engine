## Context

See proposal.md for why. Slice 2 compile already produces a live Pass order (`frame_graph.cpp`). There is no `setExecute` / `execute()`. Domain: [CONTEXT.md — Frame graph execute](../../../CONTEXT.md). Decision: [ADR 0061](../../../docs/adr/0061-frame-graph-cpu-setup.md) (extend; do not add 0062).

Keep no RHI includes, same-object mutate, `frame_graph_test` linked to `engine_runtime` only.

## Goals / Non-Goals

**Goals:**

- Store `void()` callbacks on Passes; run them in compiled live order.
- Dirty the last compile on any Setup mutation so execute cannot use a stale live order.
- Distinct execute failure `NotCompiled`; tests for the five User stories.

**Non-Goals:**

- Handle→RHI resolve, GPU alloc, alias, barriers, command recording, ForwardRenderPath, Job scheduling, catching callbacks, query sentinels, new ADR.

## Decisions

1. **`FrameGraphExecuteResult` / `FrameGraphExecuteReason::{Ok, NotCompiled}`**  
   Same `{ok, reason}` shape as compile, separate enum so execute cannot return `InvalidPass` / `NoSink`.  
   *Alternatives:* reuse `FrameGraphCompileResult` (Grill: do not echo compile reasons); throw (rejected).

2. **`std::function<void()>` on `Pass`**  
   `GraphBuilder::setExecute(pass, fn)` last-wins. Empty function is “no callback” (empty run).  
   *Alternatives:* template builder that cannot store heterogeneous callbacks without type erasure anyway; a required callback on `addPass` (Grill: name-only `addPass` stays).

3. **Compiled flag plus `clearCompileOutput` on every Setup mutation**  
   `create` / `import` / `addPass` / `read` / `write` / `markSink` / `setExecute` clear live order and lifetimes and mark not-compiled. `compile()` Ok sets the flag. Execute checks the flag only; it does not walk Setup.  
   *Alternatives:* leave stale `livePasses()` until next compile (rejected: execute must not use that order, and queries would lie); implicit compile inside execute (rejected).

4. **Callbacks must not throw; execute does not catch**  
   Graph-state failures use `{ok, reason}`. A throwing callback unwinds; remaining live Passes do not run. No `CallbackFailed`.  
   *Alternatives:* catch and return a new reason (rejected in Grill).

5. **Extend ADR 0061**  
   Add considered-options for CPU callbacks vs Packt-in-compile, dirty vs stale execute, NotCompiled vs echoing compile reasons, and no-catch.  
   *Alternatives:* ADR 0062 (rejected in Grill).

## Risks / Trade-offs

- [Throwing callback leaves later Passes unrun] → Contract is must-not-throw; tests do not throw. GPU recording is a later knife.
- [`setExecute` after compile dirties even when topology is unchanged] → Intentional; callback identity is Setup. Recompile is cheap on this CPU graph.
- [Empty live order after dirty looks like a failed compile] → Execute still returns `NotCompiled`, not a compile reason. Caller recompiles.

## Migration Plan

1. Add execute types + `setExecute` + dirty flag; extend `frame_graph_test`. Patch ADR 0061.
2. No scene, shader, or tick-path migration.
3. Rollback: remove execute APIs; compile-only graph remains.

## Open Questions

None.
