## Why

The engine tick is still a single thread, and the next CPU-heavy callers (Animation Pipeline batches, later Physics) need a process-wide scheduler. Grill rejected Fibers: first work is run-to-completion Jobs over Job data, not wait-inside-Job. Decision: [ADR 0057](../../../docs/adr/0057-task-job-system.md); domain: [CONTEXT.md — Job scheduling](../../../CONTEXT.md).

## What Changes

- Add a **Job System** Context System: dedicated Workers plus wait-help on the **Job owner thread**; fork-join of independent Jobs at a **Job barrier**. No Fiber runtime, no Job-to-Job graph, no C-ABI / Blunder.Api Submit.
- Mount it for Editor Session and Player (including Headless). Not Privileged core, not a Seam, not Editor-only.
- Jobs read/write caller-owned **Job data** only. They do not throw, return status, wait, Submit, or call Object / ClassDB / SceneInstance / RHI / Slint.
- Dedicated Worker count defaults to `max(0, hardware_concurrency - 1)`. **Zero dedicated Workers is legal** and is a required test configuration.
- First-party tests (`job_system_test`) are the v1 caller. Do not rewire Animation Pipeline, CPU skinning, or Physics.

## User stories

1. I run `job_system_test` with 0 dedicated Workers: I submit a batch of independent Jobs that write caller buffers, enter the Job barrier, and the batch finishes on the test thread. The process does not hang.
2. I run the same tests with the default Worker count: the batch still finishes; only the test thread Submits and Waits.
3. I start Headless Editor and Headless Player: both processes have a Job System. Opening a scene and ticking Play still looks and behaves as today (animation and physics are unchanged).
4. I quit those processes: they exit without hanging on Worker join.
5. There is no Inspector, C#, or C-ABI way to schedule a Job. A Behaviour Tick still runs on the engine tick thread.

## Capabilities

### New Capabilities

- `job-system`: Task Job System as a Context System — Jobs, Job data, Workers, Job owner thread, Job barrier, wait-help, isolation, no error channel, tests including zero dedicated Workers.

### Modified Capabilities

- *(none — Headless composition, Animation Pipeline, Physics, and C-ABI requirements stay as they are; this change adds a System beside them.)*

## Impact

- **Engine:** new Job System under `engine/src/runtime/function/job/`; `RuntimeGlobalContext` start/shutdown; `engine/src/tests/job_system_test.cpp`.
- **Tests:** `job_system_test` (no Vulkan / Slint). Headless boot stories use existing editor/player Headless launch, not a windowed smoke.
- **Docs:** CONTEXT Job scheduling already from Grill; [ADR 0057](../../../docs/adr/0057-task-job-system.md).
- **Non-goals:** Fiber / coroutine Jobs; Job-to-Job DAG; any-thread Complete; C-ABI or Blunder.Api schedule; Animation Pipeline / CPU skinning / Physics as callers; replacing spdlog, efsw, SDL dialog, or thumbnail queues.
