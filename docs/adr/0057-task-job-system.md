# Task Job System, not fibers

The engine needs a process-wide CPU scheduler before any tick path is parallel. We decided on a **Task Job System**: run-to-completion **Jobs** over caller-owned **Job data**, fork-join at a **Job barrier**, **wait-help** on the **Job owner thread**. It is a **Context System** (Editor and Player, including Headless), not Privileged core and not a Seam. v1 ships the System plus tests and does not rewire Animation Pipeline, CPU skinning, or Physics. Domain terms: [CONTEXT.md — Job scheduling](../../CONTEXT.md).

**Status:** accepted

## Considered Options

- **Fiber (or coroutine) Jobs that yield while waiting** — rejected for v1. First work is short CPU with a phase barrier, not wait-inside-Job. Fibers collide with Slint, Vulkan, SDL, and CoreCLR TLS; Cook / Import / thumbnails already have dedicated queues.
- **Job-to-Job dependency graph in v1** — rejected. The caller sequences independent batches with a barrier. A graph with no tick caller is a second unused scheduler.
- **Jobs that call Object, ClassDB, SceneInstance, RHI, or Slint** — rejected. Isolation stays on Job data. `AnimationTree::advance` and Physics Kernel v0 stay off the pool.
- **Animation Pipeline, CPU skinning, or Physics as the first caller** — rejected. Those paths are not Job data yet (ADR 0032 deferred world-batch animation; Physics v0 is single-threaded lockstep). First change is the System plus tests, including **zero dedicated Workers** so wait-help cannot deadlock.
- **Registered System / Editor-only mount** — rejected. Player would lack the same scheduler when a later caller lands.
- **Wait that only sleeps, or any-thread Submit/Complete** — rejected. The waiting owner thread helps. v1 has one owner thread (boot: engine tick thread; tests: the constructing thread). Dedicated Workers do not Submit or Wait. Default dedicated count is `max(0, hardware_concurrency - 1)`; zero is legal.
- **Job error codes, caught exceptions, or a C-ABI / Blunder.Api schedule surface in v1** — rejected. A Job has no error channel; a crash is a process fault. Behaviours do not schedule Jobs in this slice.
