# Manual checklist — frame-graph-data-structure

One row per User story. Agent QC does not substitute. Only the human confirms.

**Status:** Not run

Stories 1–5 are Headless Test runs (`frame_graph_test`). Test Project is not required. No windowed chrome change.

| # | User story | Pass |
|---|------------|------|
| 1 | I run a test that builds “write a Transient, then sample it from a Sink” with GraphBuilder: compile yields that order, and the Transient’s lifetime covers both Passes. | |
| 2 | I add a post-process Pass that no Sink uses: compile drops that Pass and its Transient; the Sink subgraph stays. | |
| 3 | I import an External stand-in for viewport color and write it from a Sink Pass: compile keeps it as External and does not treat it as a Transient the graph would allocate. | |
| 4 | I compile a cycle, a read/write of a Resource that was never created or imported, or a graph with no Sink: compile returns failure, does not throw, and the live Pass order is empty. | |
| 5 | I create a Buffer Transient, write it, then read it: compile tracks edges and lifetime the same way as a Texture (still no GPU allocation). | |
