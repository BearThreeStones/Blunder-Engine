# Manual checklist — frame-graph-cpu-execute

One row per User story. Agent QC does not substitute. Only the human confirms.

**Status:** Not run

Stories 1–5 are Headless Test runs (`frame_graph_test`). Test Project is not required. No windowed chrome change.

| # | User story | Pass |
|---|------------|------|
| 1 | I run a test with Clear then Forward (Sink) plus a Pass that DCE drops; all three have `setExecute`: `execute()` succeeds and runs Clear then Forward only; the culled callback never runs. | |
| 2 | I leave a live Sink without a callback: `execute()` still succeeds as an empty run; live Passes that have callbacks still run. | |
| 3 | I call `execute()` when compile has never succeeded, or the last compile failed: execute returns `NotCompiled`, runs no callbacks, does not compile, and does not echo `InvalidPass` / `NoSink` / other compile reasons. | |
| 4 | After a successful compile I mutate Setup (including `setExecute`) and execute without compiling again: `NotCompiled`, no callbacks. After compile succeeds again, execute runs; the same successful compile may execute twice and each callback runs twice. | |
| 5 | I call `setExecute` on a Pass handle that was never added: compile fails with `InvalidPass` and an empty live order. | |
