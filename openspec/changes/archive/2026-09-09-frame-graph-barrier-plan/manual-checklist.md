# Manual checklist — frame-graph-barrier-plan

One row per User story. Agent QC does not substitute. Only the human confirms.

**Status:** Not run

Stories 1–5 are Headless Test runs (`frame_graph_test`). Test Project is not required. No windowed chrome change.

| # | User story | Pass |
|---|------------|------|
| 1 | I compile Clear then Forward, both Write the same Transient as ColorAttachment, then `planBarriers()` without allocate or execute: the table has a WAW row, from and to both Write+ColorAttachment, `after` is Clear, `before` is Forward. | |
| 2 | I compile a live Transient Texture plus an External dummy on the same graph: Transient first use is Undefined → first Pass entering state with invalid `after`; the External has no Undefined→… row. A DCE’d Transient has no rows. | |
| 3 | I Write ColorAttachment then a later live Pass Reads Sampled: RAW, from Write+Color to Read+Sampled. Two consecutive Sampled Reads: no barrier between them. | |
| 4 | I compile a live Transient Buffer size 256, Write Storage then Read Storage: RAW, same rules as Texture. | |
| 5 | I call `planBarriers()` with no compile: `NotCompiled`, `barriers()` empty. I compile then execute without the plan: still Ok (CPU callbacks, after allocate). I `planBarriers()` twice: the list is unchanged. I mutate Setup: `barriers()` empty; compile + `planBarriers()` again restores the table. | |
