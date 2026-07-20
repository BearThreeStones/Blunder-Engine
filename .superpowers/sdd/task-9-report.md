# Task 9 Report — OpenSpec 3.1 Formalize freshness / Cook API

**Status:** done  
**Branch:** `feat/asset-pipeline-pull`  
**Worktree:** `e:\Dev\Blunder-Engine\.worktrees\asset-pipeline-pull`  
**Commit:** `5df42cb` — feat: formalize AssetCompilerService Pull freshness Cook API

## Summary

`AssetCompilerService` now exposes Pull freshness APIs:

| API | Behavior |
|-----|----------|
| `markFinalStale(guid)` | Deletes `.meshbin`/`.meshbin.meta` and `.texbin`/`.texbin.meta` under `.blunder/cooked/` for the GUID |
| `cookAsset(guid, force=false)` | Resolves descriptor via `AssetRegistry`, cooks mesh or texture; returns true only when a Final was written |
| `cookDependents(guid)` | Stub no-op until dependency graph (4.x); documented in header |
| `cookIfStale()` | Unchanged warm-up: still `cookAll(false)` with clarifying comments |

Private `cookMeshDescriptor` / `cookTextureDescriptor` remain the implementation path for both cook-one and cook-all.

## TDD evidence

### RED (API missing)

`asset_compiler_freshness_test` called `markFinalStale` / `cookAsset` / `cookDependents` before they existed.

Build failed with MSVC `C2039`, e.g.:

```
error C2039: "markFinalStale": not a member of "Blunder::AssetCompilerService"
error C2039: "cookAsset": not a member of "Blunder::AssetCompilerService"
error C2039: "cookDependents": not a member of "Blunder::AssetCompilerService"
```

Logs: `.superpowers/sdd/task-9-red-build.txt`, `.superpowers/sdd/task-9-red-excerpt.txt`

### GREEN (implementation)

- `asset_compiler_service.h` — public freshness API + warm-up docs on `cookIfStale`
- `asset_compiler_service.cpp` — `removeIfExists`, `markFinalStale`, `cookAsset`, stub `cookDependents`
- `engine/src/tests/asset_compiler_freshness_test.cpp` + CMake wiring

Build: `cmake --build build/vs2026-debug --config Debug --target asset_compiler_freshness_test` → succeeded  
Run: `asset_compiler_freshness_test.exe` → **exit 0** (`asset_compiler_freshness_test: all passed`)

Logs: `.superpowers/sdd/task-9-green-build.txt`, `.superpowers/sdd/task-9-green-run.txt`

## Test coverage

| Case | Result |
|------|--------|
| `markFinalStale` deletes fake mesh bin+meta | pass |
| `markFinalStale` deletes fake texture bin+meta | pass |
| `cookAsset` cooks registered mesh; skips when fresh | pass |
| `markFinalStale` then `cookAsset` recooks; `cookDependents` callable | pass |
| `cookAsset(..., force=true)` recooks fresh Final | pass |

## OpenSpec tasks

- [x] 3.1 in `openspec/changes/asset-pipeline-pull/tasks.md`

## Concerns / notes

1. `cookDependents` is intentionally a no-op until 4.x; do not rely on fan-out yet.
2. `markFinalStale` removes both mesh and texture Finals for a GUID (type-agnostic); unknown GUID is a silent no-op.
3. `cookAsset` returns false for unknown GUID, unsupported descriptor type, skip-when-fresh, and cook failure (same boolean shape as private cook helpers).
4. Texture cook-one is wired but not unit-tested here (would need image source + load path); mesh path covers registry resolve + freshness.
5. Runtime-linked tests need `slint_cpp.dll` on `PATH` (cargo debug dir under `build/vs2026-debug/x64/Debug/cargo/...`).
6. Did not push. Did not implement 3.2 Fast Path load behavior.
