# Task 16 Report — OpenSpec 4.5 Watch/invalidation/Reimport tests gate

**Status:** done  
**Branch:** `feat/asset-pipeline-pull`  
**Worktree:** `e:\Dev\Blunder-Engine\.worktrees\asset-pipeline-pull`  
**Commit:** `973d4d1` — test: cover watch invalidation and Reimport gate (4.5)

## Summary

Audited coverage from tasks 4.1–4.4. Partial evidence already existed (GUID invalidate dependents, path→GUID map, archived_source map, `requestReimports` batch). Added end-to-end composition tests for the three 4.5 themes where path→action→Final was missing, then marked `[x] 4.5`.

## Coverage audit

| Theme | Before | After |
|-------|--------|-------|
| 1. Texture change → mesh Final stale | `invalidateMarksSelfAndDependents` (by GUID); Intermediate map only | + Intermediate texture path → `guidsToInvalidate` → `invalidateAssetAndDependents` → mesh Final gone |
| 2. Descriptor change → Final stale | Descriptor map only; solo GUID invalidate | + Descriptor path → map → invalidate → own Final gone |
| 3. Source change → Reimport hook | `guidsForArchivedSourcePath` + batch rebuild count | + Source path → `findGuidsByArchivedSource` → `requestReimports` → mesh Final gone |

## New tests

### `asset_watch_path_test`

- `intermediateTextureChangeInvalidatesMeshFinal`
- `descriptorChangeInvalidatesFinal`
- `sourceChangeTriggersReimportInvalidatesFinal`

### `asset_compiler_invalidate_test` (reinforcing)

- `textureIntermediateChangeInvalidatesMeshFinal`
- `descriptorChangeInvalidatesOwnFinal`

## Verification

Preset: `build/vs2026-debug` Debug. PATH includes Slint DLL dirs.

| Target | Result |
|--------|--------|
| `asset_watch_path_test` | exit 0 (`asset_watch_path_test: all passed`) |
| `asset_compiler_invalidate_test` | exit 0 (`asset_compiler_invalidate_test: all passed`) |

Logs: `.superpowers/sdd/task-16-watch-run.txt`, `.superpowers/sdd/task-16-invalidate-run.txt`

## OpenSpec tasks

- [x] 4.5 in `openspec/changes/asset-pipeline-pull/tasks.md`

## Concerns / notes

1. Composition tests drive the same helpers the watch debounce path uses; they do **not** exercise live efsw / debounce timing (same as 4.3/4.4).
2. `requestReimport` remains a stub (invalidate Finals; full Assimp Source Export dual-write is 5.3).
3. Theme 1/2 appear in both binaries for gate clarity; slightly redundant but intentional.
4. Did not push.
