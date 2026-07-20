# Task 3.2 Report — Intermediate Upgrade fail-soft

**Status:** DONE  
**Branch:** `feat/asset-pipeline-pull`  
**Worktree:** `e:\Dev\Blunder-Engine\.worktrees\asset-pipeline-pull`

## Summary

Fail-soft Intermediate Upgrade on Assimp convert failure:

- Descriptor / `source` left unchanged (legacy glTF/GLB)
- Sibling partial `.dae` removed so `source` never points at a partial Intermediate
- Descriptor write failure also removes the new `.dae`
- Legacy Fast Path `loadMesh` still works while upgrade is pending

## TDD evidence

### RED

`lazyIntermediateUpgradeFailSoftLeavesLegacySource` with injected convert failure + planted partial `hero.dae` → **FAIL fail-soft: cleans partial sibling .dae** (descriptor/loadMesh already OK).

Logs: `task-ci-8-red-build.txt`, `task-ci-8-red-run.txt`, `task-ci-8-red-excerpt.txt`

### GREEN

On convert failure: remove sibling `.dae` before continue; on descriptor write failure: remove new `.dae`.

Build: `cmake --build build/vs2026-debug --config Debug --target asset_import_test` → succeeded  
Run: `asset_import_test.exe` → **all passed** (exit 0)

Logs: `task-ci-8-green-build.txt`, `task-ci-8-green-run.txt`

## Changes

| File | Change |
|------|--------|
| `asset_import_service.h/.cpp` | Fail-soft partial cleanup; test seam `setForceUpgradeConvertFailureForTest` |
| `asset_import_test.cpp` | `lazyIntermediateUpgradeFailSoftLeavesLegacySource` |
| `openspec/.../tasks.md` | `[x] 3.2` |

## Tests

`asset_import_test` — fail-soft: source stays `.gltf`, partial `.dae` cleaned, `loadMesh` returns 3 verts via legacy Fast Path

## Concerns

- Convert failure is injected via a test seam (keeps Intermediate glTF loadable); real Assimp read failures are covered by the same cleanup path
- Did not push
