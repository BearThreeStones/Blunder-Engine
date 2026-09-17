# Manual checklist — frame-graph-gpu-execute

One row per User story. Agent QC does not substitute. Only the human confirms.

**Status:** Not run

Stories 1–5 are Headless Test runs (`frame_graph_test`). Test Project is not required. No windowed chrome change.

| # | User story | Pass |
|---|------------|------|
| 1 | I compile Clear then Forward, both Write the same Transient as ColorAttachment, allocate and `planBarriers()`, then `execute(dummy)`: Dummy gets the first-use row, then the Clear callback, then the WAW row, then the Forward callback. The Forward callback’s recorder is the same Dummy I passed to `execute()`. | |
| 2 | I compile and allocate, skip `planBarriers()`, then `execute(dummy)`: `NotPlanned`. Dummy has no rows. No callback runs. | |
| 3 | I have only an External Sink Write (empty plan): `planBarriers()` Ok, `execute(dummy)` Ok. Dummy calls `pipelineBarrier` zero times. The Sink callback runs once. | |
| 4 | I Write ColorAttachment then a later live Pass Reads Sampled: the RAW row is recorded before the reader callback. Two consecutive Sampled Reads: no barrier between them. A Transient Buffer Storage Write then Read: RAW before the reader, same rules as Texture. | |
| 5 | I `execute(dummy)` with no compile: `NotCompiled`. I compile and skip allocate (and skip plan): `NotAllocated`. After compile + allocate + plan, a second `execute(dummy)` without Setup mutation records the same sequence. After Setup mutation, execute is `NotCompiled` and records nothing. | |
