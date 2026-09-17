# Manual checklist — task-job-system

One row per User story. Agent QC does not substitute. Only the human confirms.

**Status:** Not run

Stories 1–2 are Headless Test runs (`job_system_test`). Stories 3–5 are Headless Editor/Player boot (no windowed chrome change). Test Project is not required for 1–2.

| # | User story | Pass |
|---|------------|------|
| 1 | I run `job_system_test` with 0 dedicated Workers: I submit a batch of independent Jobs that write caller buffers, enter the Job barrier, and the batch finishes on the test thread. The process does not hang. | |
| 2 | I run the same tests with the default Worker count: the batch still finishes; only the test thread Submits and Waits. | |
| 3 | I start Headless Editor and Headless Player: both processes have a Job System. Opening a scene and ticking Play still looks and behaves as today (animation and physics are unchanged). | |
| 4 | I quit those processes: they exit without hanging on Worker join. | |
| 5 | There is no Inspector, C#, or C-ABI way to schedule a Job. A Behaviour Tick still runs on the engine tick thread. | |
