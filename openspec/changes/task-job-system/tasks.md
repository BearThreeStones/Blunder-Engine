## 1. JobSystem

- [x] 1.1 Add `engine/src/runtime/function/job/job_system.h` and `job_system.cpp`: function-pointer + `void*` Job, initialize with dedicated Worker count (default `max(0, hardware_concurrency - 1)`), Submit, Job barrier with wait-help, index-range helper, shutdown that joins Workers. Capture owner thread id at initialize. Abort if a Job throws.
- [x] 1.2 Wire the sources into `engine/src/runtime/CMakeLists.txt`.

## 2. Tests

- [x] 2.1 Add `engine/src/tests/job_system_test.cpp`: 0 dedicated Workers — Submit independent buffer writes, barrier returns, no hang; default Worker count — same; shutdown after a barrier joins; index-range helper writes every slot.
- [x] 2.2 Wire `job_system_test` in `engine/src/tests/CMakeLists.txt` (link `engine_runtime` only; no Vulkan / Slint).
- [x] 2.3 Build and run `job_system_test` (`build/vs2026-debug`, Debug).

## 3. Context System mount

- [x] 3.1 Add `m_job_system` on `RuntimeGlobalContext`; `startSystems` initializes it after logger (both Editor and Player, including Headless); `shutdownSystems` shuts it down on the owner thread before process exit (join Workers; after systems that must not run Jobs — none in v1, so late teardown next to other Context Systems is enough).

## 4. Docs

- [x] 4.1 If code names differ from CONTEXT Job scheduling / ADR 0057, align the glossary only; do not expand v1 into Fibers, DAG, C-ABI, or animation/physics callers.
