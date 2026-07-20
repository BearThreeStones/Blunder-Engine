# Task 13 Report — OpenSpec 4.2 Propagate Final invalidation

**Status:** done  
**Branch:** `feat/asset-pipeline-pull`  
**Worktree:** `e:\Dev\Blunder-Engine\.worktrees\asset-pipeline-pull`  
**Commit:** (see git log after commit)

## Summary

Wired `AssetDependencyGraph` into `AssetCompilerService` and propagated Final invalidation along reverse edges.

| API | Behavior |
|-----|----------|
| `rebuildDependencyGraph()` | Rebuilds held graph from `FileSystem` + `AssetRegistry` |
| `invalidateAssetAndDependents(guid)` | Transitive BFS: `markFinalStale` on guid + all `dependentsOf` chain |
| `cookDependents(guid)` | Upgraded from stub: `cookAsset` on **direct** dependents |

## TDD evidence

### RED

`asset_compiler_invalidate_test` called `rebuildDependencyGraph` / `invalidateAssetAndDependents` before they existed → MSVC `C2039`.

Logs: `.superpowers/sdd/task-13-red-build.txt`, `.superpowers/sdd/task-13-red-excerpt.txt`

### GREEN

Implementation + graph fixtures. `asset_compiler_invalidate_test` → **exit 0**.

Logs: `.superpowers/sdd/task-13-green-build.txt`, `.superpowers/sdd/task-13-green-run.txt`

Regression: `asset_compiler_freshness_test` → **exit 0**.

## Test coverage

| Case | Result |
|------|--------|
| Texture→Mesh fixture: invalidate texture removes tex + mesh Finals | pass |
| Solo texture (no dependents): invalidate removes only self | pass |
| After texture invalidate, `cookDependents` restores mesh Final | pass |
| Existing freshness suite | pass |

## OpenSpec tasks

- [x] 4.2 in `openspec/changes/asset-pipeline-pull/tasks.md`

## Concerns / notes

1. Callers must `rebuildDependencyGraph()` before invalidate/cookDependents so the held graph is current (watch path will own rebuild cadence in 4.3+).
2. Invalidation walks the reverse-edge chain transitively; `cookDependents` cooks **direct** dependents only (scene dependents get a cookAsset unsupported-descriptor warn).
3. `markFinalStale` remains type-agnostic (mesh+texture Finals for a GUID).
4. Runtime-linked tests need `slint_cpp.dll` on `PATH` (cargo debug dir under `build/vs2026-debug/x64/Debug/cargo/...`).
5. Did not push.
