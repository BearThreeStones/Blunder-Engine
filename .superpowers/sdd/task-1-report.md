# Task 1 Report: Native MessageDispatch + unit tests

## Status

DONE

## Summary

Implemented `MessageDispatch` — a static registry mapping message names to monotonic `MessageId` values, plus synchronous fan-out to non-null Behaviour script peers on a target `Object`. Added unit test covering registration stability, peer fan-out order, invalid-target no-op, and argc/id validation.

## TDD Evidence

### RED (test before implementation)

**Command:**
```powershell
cmake --preset vs2026-debug
cmake --build build/vs2026-debug --config Debug --target message_dispatch_test
```

**Result:** exit code 1 — missing header (expected).

```
E:\Dev\Blunder-Engine\engine\src\tests\message_dispatch_test.cpp(2,1): error C1083: 无法打开包括文件: "runtime/core/reflection/message_dispatch.h": No such file or directory
```

Saved: `.superpowers/sdd/task-1-red-build.txt`

### GREEN (after implementation)

**Command:**
```powershell
cmake --build build/vs2026-debug --config Debug --target message_dispatch_test
.\build\vs2026-debug\engine\src\tests\Debug\message_dispatch_test.exe
```

**Result:** build exit 0, test exit 0 (silent pass).

Saved: `.superpowers/sdd/task-1-green-run.txt`

## Files Changed

| File | Action |
|------|--------|
| `engine/src/runtime/core/reflection/message_dispatch.h` | Created — `MessageId`, `MessageArg`, `MessageDispatch` API |
| `engine/src/runtime/core/reflection/message_dispatch.cpp` | Created — registry, hook, sync fan-out |
| `engine/src/tests/message_dispatch_test.cpp` | Created — unit test from brief |
| `engine/src/runtime/CMakeLists.txt` | Modified — added `message_dispatch.cpp` next to `lifecycle.cpp` |
| `engine/src/tests/CMakeLists.txt` | Modified — added `message_dispatch_test` target after `ptrcall_lifecycle_test` |

## Commit

```
f9e1a13 feat: add MessageDispatch registry and sync fan-out
```

## Self-Review

**Correctness**
- `registerName` uses `eastl::unordered_map<eastl::string, MessageId>` with monotonic IDs starting at 1; duplicate names return the same ID.
- `send` returns `false` when `argc` ∉ [0,4] or `id == 0`; invalid `ObjectId` returns `true` without calling the hook.
- Fan-out copies `BehaviourId`s into a local vector before iteration (safe if hook mutates behaviours), re-gets object each iteration, skips null peers — mirrors `LifecycleDispatch::invokeTick` pattern.
- Null hook is a no-op success (consistent with lifecycle when no hook registered).

**Test coverage**
- Registration: non-zero, stable, distinct names.
- Fan-out: 3 behaviours (A/B/C), only peers 1 and 3 called in order.
- Args forwarded: argc=2, `args[0].i == 42`.
- Invalid target `ObjectId(0)`: success, no hook calls.
- Validation: argc=5 fails, id=0 fails.

**Scope**
- Task 1 only — no C-ABI, NativeAbi, or C# façade.
- No ABI version bump.

**Minor notes (non-blocking)**
- `MessageArg` union has no explicit default ctor beyond `kind{Nil}`; MSVC accepts brace-init in tests (`MessageArg too_many[5]{}`).
- `registerName(nullptr)` returns 0 — not exercised by test but safe.

## Concerns

None.

## Review Fix (CMake scope)

**Issue:** Commit `f9e1a13` bundled unrelated project-manager CMake wiring (runtime sources, `project_manager.slint`, and six project-manager test targets) into Task 1.

**Fix commit:** `fb8bd74` — `fix: keep MessageDispatch CMake changes task-scoped`

**Removed from Task 1 CMake delta:**
- `engine/src/runtime/CMakeLists.txt`: `project_list`, `project_relaunch`, `project_manager_controller`, `project_manager_app` sources; `slint_target_sources` for `project_manager.slint`
- `engine/src/tests/CMakeLists.txt`: `project_file_test`, `project_list_test`, `project_manager_controller_test`, `project_relaunch_test`, `project_last_opened_display_test`, `editor_launch_test`

**Retained:** `message_dispatch.h/.cpp` in `engine_runtime`; `message_dispatch_test` target only.

**Note:** Project-manager sources remain in the working tree (untracked/uncommitted) for a separate change; they are not required for `message_dispatch_test` to configure or build.

**Re-verify (post-fix):**
```powershell
cmake --build build/vs2026-debug --config Debug --target message_dispatch_test
.\build\vs2026-debug\engine\src\tests\Debug\message_dispatch_test.exe
```

**Result:** build exit 0, test exit 0 (silent pass).
