# Task 4 Report — OpenSpec 2.2 Scene registration in AssetRegistry

**Status:** done  
**Branch:** `feat/asset-pipeline-pull`  
**Worktree:** `e:\Dev\Blunder-Engine\.worktrees\asset-pipeline-pull`  
**Commit:** `9714006` — feat: register scene assets in AssetRegistry

## Summary

`AssetRegistry::rebuildFromScan` now registers `*.scene.asset` files that already contain a valid `guid` (JSON parsed via yaml-cpp, same as YAML descriptors). Added `ensureSceneAssetRegistered` to allocate+persist a guid for legacy scenes and register scenes that already have one.

## TDD evidence

### RED (feature missing)

Test `asset_registry_test` called `AssetRegistry::ensureSceneAssetRegistered` before the API existed.

Build failed with MSVC `C2039` (member not found), e.g.:

```
asset_registry_test.cpp(108,24): error C2039: "ensureSceneAssetRegistered": 不是 "Blunder::AssetRegistry" 的成员
asset_registry_test.cpp(155,16): error C2039: "ensureSceneAssetRegistered": 不是 "Blunder::AssetRegistry" 的成员
```

Full log: `.superpowers/sdd/task-4-red-build.txt`  
Excerpt: `.superpowers/sdd/task-4-red-excerpt.txt`

### GREEN (implementation)

Minimal production changes:

- `asset_registry.h` — document scene scan; declare `ensureSceneAssetRegistered`
- `asset_registry.cpp` — scan `.scene.asset`; parse guid from JSON/YAML; ensure helper allocates/persists/registers
- `engine/src/tests/asset_registry_test.cpp` + CMake wiring

Build: `cmake --build build/vs2026-debug --config Debug --target asset_registry_test` → succeeded  
Run (with slang/slint DLLs on PATH): `asset_registry_test.exe` → **exit 0** (`asset_registry_test: all passed`)

Log: `.superpowers/sdd/task-4-green-build.txt`, `.superpowers/sdd/task-4-green-run.txt`

## Test coverage

| Case | Result |
|------|--------|
| `rebuildFromScan` registers `.scene.asset` that already has guid | pass |
| `ensureSceneAssetRegistered` allocates guid for legacy scene, persists file, registry resolves | pass |
| `ensureSceneAssetRegistered` registers scene that already has guid when missing from registry | pass |

## OpenSpec tasks

- [x] 2.2 in `openspec/changes/asset-pipeline-pull/tasks.md`

## Concerns / notes

1. Guid insert into legacy scene JSON is a string splice after `"type"` (preserves entities/childScenes); full SceneSerializer guid read/write is task 2.3.
2. Scan still skips scenes without a valid guid (upgrade is explicit via `ensureSceneAssetRegistered`).
3. Tests need a `LogSystem` on `g_runtime_global_context` because `FileSystem::initialize` logs; logger is reset before exit to avoid spdlog async teardown ACCESS_VIOLATION.
4. Runtime-linked tests need `slang.dll` and `slint_cpp.dll` on `PATH`.
5. Did not implement mesh GUID migration (2.3). Did not push.

---

## Review fix — invalid guid rewrite

**Finding:** `insertGuidIntoSceneJson` returned true when a `"guid"` key already existed (even if invalid), so `ensureSceneAssetRegistered` registered a new GUID while disk kept the bad value.

**Fix commit:** `906526a` — fix: rewrite invalid scene guid on ensure register

### RED

Regression `ensureRewritesInvalidGuidOnDisk` with `"guid": "not-a-valid-guid"`:

```
FAIL disk no longer contains invalid guid
FAIL disk contains recovered guid matching registry
2 failure(s)
```

Log: `.superpowers/sdd/task-4-fix-red-run.txt`

### GREEN

Renamed helper to `upsertGuidIntoSceneJson`: replaces existing `"guid"` string value when present; inserts when absent. `ensureSceneAssetRegistered` still allocates on parse failure and writes the upserted document.

`asset_registry_test.exe` → **exit 0** (`asset_registry_test: all passed`)

Logs: `.superpowers/sdd/task-4-fix-green-build.txt`, `.superpowers/sdd/task-4-fix-green-run.txt`

### Test coverage (added)

| Case | Result |
|------|--------|
| `ensureSceneAssetRegistered` rewrites invalid on-disk guid; disk matches registry | pass |
