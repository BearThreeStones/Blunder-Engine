# Manual checklist — frame-graph-compile-write-order

One row per User story. Agent QC does not substitute. Only the human confirms.

**Status:** Not run

Stories 1–5 are Headless Test runs (`frame_graph_test`). Test Project is not required. No windowed chrome change.

| # | User story | Pass |
|---|------------|------|
| 1 | I run a test where Clear and Forward both Write the same Transient as ColorAttachment and Forward is the Sink: compile keeps both Passes in that order, and the Transient’s lifetime covers both. | |
| 2 | I run a test that Writes a resource, Reads it from an SSAO Pass, then Writes it again from a TAA Sink: compile keeps SSAO before TAA. | |
| 3 | I run a test where A and B both Read a resource and C Writes it as the Sink: compile keeps A and B live and before C; A and B are not ordered against each other. | |
| 4 | I call `read`, `write`, or `markSink` with a Pass handle that was never added: compile fails with `InvalidPass` and an empty live order. A never created or imported Resource still fails as `DanglingAccess`. If several failures apply, I get one reason: `InvalidPass`, then `DanglingAccess`, then `NoSink`, then `Cycle`. | |
| 5 | I run a test where one Sink Pass Writes then Reads the same handle (or Writes it twice): compile succeeds and does not report `Cycle`. | |
