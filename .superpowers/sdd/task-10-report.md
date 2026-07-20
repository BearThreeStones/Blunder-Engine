# Task 10 Report — OpenSpec 3.2 Fast Path load + request Cook

**Status:** done  
**Branch:** `feat/asset-pipeline-pull`  
**Worktree:** `e:\Dev\Blunder-Engine\.worktrees\asset-pipeline-pull`  
**Commit:** `9eaf9b7` — feat: Fast Path Intermediate load requests cookAsset

## Summary

Formalized the existing Intermediate fallback as **Fast Path** and wired `AssetManager` to request `cookAsset(guid)` after a successful Intermediate load when Final is missing/stale (mesh and texture).

| Piece | Behavior |
|-------|----------|
| Fresh Final | Unchanged: load cooked bin when meta is fresh |
| Missing/stale Final | Load Intermediate source, then `requestCookAfterFastPath(guid)` |
| Coupling | `AssetManager::setAssetCompiler(weak_ptr)` — no ownership of compiler |
| Init order | `global_context`: initialize compiler → `setAssetCompiler` → `cookIfStale` |
| Shutdown | Clear weak_ptr on manager before compiler shutdown |
| Sync cook (v1) | Documented: Intermediate is already loaded; cook runs sync so next load can prefer Final. Reentrancy flag prevents `cookMeshDescriptor` → `loadMesh` recursion |

## TDD evidence

### RED (API missing)

`asset_manager_fast_path_test` called `setAssetCompiler` before it existed.

Build failed with MSVC `C2039`:

```
error C2039: "setAssetCompiler": not a member of "Blunder::AssetManager"
```

Logs: `.superpowers/sdd/task-10-red-build.txt`, `.superpowers/sdd/task-10-red-excerpt.txt`

### GREEN (implementation)

- `asset_manager.h` / `.cpp` — `setAssetCompiler`, `requestCookAfterFastPath`, Fast Path logs + cook request for mesh/texture descriptors
- `global_context.cpp` — wire weak_ptr after compiler init; clear before compiler teardown
- `engine/src/tests/asset_manager_fast_path_test.cpp` + CMake wiring

Build: `cmake --build build/vs2026-debug --config Debug --target asset_manager_fast_path_test` → succeeded  
Run: `asset_manager_fast_path_test.exe` → **exit 0** (`asset_manager_fast_path_test: all passed`)

Logs: `.superpowers/sdd/task-10-green-build.txt`, `.superpowers/sdd/task-10-green-run.txt`

## Test coverage

| Case | Result |
|------|--------|
| Mesh descriptor, missing Final → returns Intermediate mesh | pass |
| Mesh Fast Path requests cook → `.meshbin` + meta written | pass |
| Texture descriptor, missing Final → returns Intermediate texture | pass |
| Texture Fast Path requests cook → `.texbin` + meta written | pass |

## OpenSpec tasks

- [x] 3.2 in `openspec/changes/asset-pipeline-pull/tasks.md`

## Concerns / notes

1. **Sync cook blocks the load call** until `cookAsset` finishes (v1 OK per brief). Async/deferred cook would better match “do not block returning Intermediate” for slow cooks; Intermediate is still what the caller receives.
2. **Reentrancy guard** is required because `cookMeshDescriptor` still calls `loadMesh(descriptor path)` (unlike texture cook, which loads `descriptor.source` directly). Nested Fast Path skips a second cook request.
3. Without `setAssetCompiler`, Fast Path Intermediate load still works; cook is simply not requested (unit tests / tools that omit the wire).
4. Task 3.3 (stale meta / fresh Final preferred assertions) remains open; this task proves missing-Final Fast Path + cook request.
5. Runtime-linked tests need `slint_cpp.dll` / `slang.dll` on `PATH`.
6. Did not push.
