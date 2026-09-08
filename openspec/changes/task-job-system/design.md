## Context

See proposal.md for why. Grill and [ADR 0057](../../../docs/adr/0057-task-job-system.md) lock Task Jobs (not Fibers), Job data isolation, Context System mount, wait-help, one owner thread, no DAG, no C-ABI. The process today has no first-party `std::thread` pool: `tickOneFrame` is serial; spdlog / efsw / SDL dialogs stay dedicated and out of this System. New Systems go under `engine/src/runtime/function/<name>/` and through `RuntimeGlobalContext` ([cmake.md](../../../docs/agents/cmake.md)).

## Goals / Non-Goals

**Goals:**

- A `JobSystem` type tests can construct with an explicit dedicated Worker count, including 0.
- The same type mounted on `RuntimeGlobalContext` for Editor and Player.
- Submit + barrier (+ optional index-range helper) that wait-help can drain.
- `job_system_test` with no Vulkan / Slint.

**Non-Goals:**

- Work-stealing deques, job stealing metrics, or per-Worker local queues as v1 requirements (a single mutex-protected queue is enough).
- Catching Job failures into `exception_ptr` or status codes.
- Wiring Animation Pipeline, CPU skinning, or Physics.

## Decisions

1. **Sources live under `function/job/`**  
   Follow the CMake new-System path. Not Privileged core (`core/` ClassDB / RHI).  
   *Alternatives:* `core/job/` (reads as kernel; ADR says it is not Privileged core).

2. **`std::thread` dedicated Workers; `std::mutex` + condition variable queue**  
   C++20, Windows and Linux. No Fiber API, no extra 3rdparty scheduler.  
   *Alternatives:* Fibers (rejected, ADR 0057); vendored enkiTS/Taskflow (new framework; Complexity penalty).

3. **Job entry is function pointer + `void*` Job data**  
   No `std::function` allocation on the submit path. Tests pass a small trampoline.  
   *Alternatives:* `std::function` (heap + exceptions); C++20 coroutine as the Job unit (yield semantics Grill rejected).

4. **Owner thread id captured at `initialize`**  
   Submit and barrier debug-assert (or hard fail in tests) if called from another thread. Dedicated Workers never call those APIs.  
   *Alternatives:* any-thread Complete (rejected for v1).

5. **Barrier is a completion counter**  
   Submit increments outstanding work; each finished Job decrements; owner help-pops until the counter is zero, then waits only if dedicated Workers still hold items.  
   *Alternatives:* per-Job handles (unneeded without DAG); dependency graph (rejected).

6. **Index-range helper is sugar over independent Jobs**  
   Split `[begin, end)` into chunks, Submit, one barrier. Not a new spec capability.  
   *Alternatives:* only raw Submit (tests would duplicate chunking).

7. **Tests construct `JobSystem` directly**  
   `job_system_test` does not call `startSystems` (that cooks and may need a Project). Host-mount stories are Human acceptance on Headless Editor/Player, plus a pointer check if a cheap composition test can see `m_job_system` without a full boot.  
   *Alternatives:* boot `startSystems` in the unit test (slow, Project-dependent).

8. **Shutdown on the owner thread joins Workers**  
   Flag stop, wake sleepers, join. Do not Submit after shutdown.  
   *Alternatives:* detach Workers (process-exit races).

9. **If a Job throws, abort**  
   Wrap invoke in try/catch and `std::abort` (or equivalent) so exceptions do not cross threads. No error channel.  
   *Alternatives:* swallow; store `exception_ptr` for the barrier (rejected).

## Risks / Trade-offs

- [Idle dedicated Workers from boot with no tick caller] → Expected for a Context System; Workers block on the queue. Cost is N-1 sleeping threads, not CPU.
- [Owner thread Submit from a Slint callback later] → v1 forbids a second Submit thread; keep Submit on the tick/owner thread. Do not “fix” by allowing any-thread Complete.
- [Job accidentally calls engine objects] → Contract + code review; v1 tests only pass raw buffers. No runtime type firewall.
- [0-Worker tests hide queue bugs] → Also run the default Worker count path in the same binary.
- [spdlog already has one async thread] → Ignore for v1 Worker math (Grill).

## Migration Plan

1. Land `JobSystem` + `job_system_test` + GlobalContext mount/shutdown.
2. No content or scene migration.
3. Rollback: revert the System and tests; no on-disk format.

## Open Questions

None.
