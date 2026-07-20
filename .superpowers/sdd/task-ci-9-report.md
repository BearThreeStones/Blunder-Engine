# Task 3.3 Report — Intermediate Upgrade / Reimport test gate

**Status:** DONE  
**Branch:** `feat/asset-pipeline-pull`  
**Worktree:** `e:\Dev\Blunder-Engine\.worktrees\asset-pipeline-pull`  
**Commit:** `a657fc8` — test: Intermediate Upgrade Reimport regenerates .dae

## Summary

Audited Task 3.3 coverage:

| Requirement | Coverage |
|-------------|----------|
| Successful upgrade glTF→`.dae` | `lazyIntermediateUpgradeGltfToDaeSuccessPath` (3.1) |
| Failed upgrade leaves legacy | `lazyIntermediateUpgradeFailSoftLeavesLegacySource` (3.2) |
| Reimport from archived Source regenerates `.dae` | Added `reimportAfterUpgradeRegeneratesDaeFromArchivedGltf` (upgrade → stamp `.dae` → Reimport from archived glTF) |

OBJ Reimport (`reimportObjPreservesGuidAndRefreshesIntermediate`) already covered Source Export Reimport; the new test gates the post-Upgrade archived-glTF path specifically.

No production code changes — existing `requestReimport` / refresh path already regenerates COLLADA.

## Evidence

Build: `cmake --build build/vs2026-debug --config Debug --target asset_import_test` → succeeded  
Run: `asset_import_test.exe` (slint DLL on PATH) → **all passed** (exit 0)

Logs: `task-ci-9-green-build.txt`, `task-ci-9-green-run.txt`

## Changes

| File | Change |
|------|--------|
| `asset_import_test.cpp` | `reimportAfterUpgradeRegeneratesDaeFromArchivedGltf` |
| `openspec/.../tasks.md` | `[x] 3.3` |

## Tests

`asset_import_test` — after upgrade: Reimport regenerates COLLADA `.dae` from archived glTF, GUID stable, Finals invalidated

## Concerns

- No RED cycle for new production code (test-only gate; behavior already shipped in 3.1/3.2 + Reimport)
- Did not push
