# Task 12 Report — OpenSpec 4.1 Minimal Asset Dependency Graph

**Status:** done  
**Branch:** `feat/asset-pipeline-pull`  
**Worktree:** `e:\Dev\Blunder-Engine\.worktrees\asset-pipeline-pull`  
**Commit:** *(pending — filled after commit)*

## Summary

Added `AssetDependencyGraph` (`rebuildFromProject` + `dependentsOf` + `intermediateLeavesOf`) and optional mesh descriptor `texture_guids` for Mesh→Texture edges. Scene→Mesh edges come from scene JSON mesh GUID refs; Intermediate leaf side-table stores descriptor virtual path + Intermediate `source` for mesh/texture (scene leaf is descriptor only).

| Edge / API | Source |
|------------|--------|
| Scene→Mesh | Scene entity `"mesh"` GUID via `SceneSerializer` |
| Mesh→Texture | Optional YAML `texture_guids` list |
| Asset→leaves | Descriptor path + Intermediate `source` (side table) |
| `dependentsOf(guid)` | Reverse of the above asset→asset edges |

## TDD evidence

### RED — `texture_guids`

Tests referenced missing `MeshAssetDescriptor::texture_guids` → compile fail (`C2039`). After adding the field (parse/serialize still absent): `FAIL texture_guids size 2`.

Log: `.superpowers/sdd/task-12-yaml-red-run.txt`

### GREEN — `texture_guids`

Parse/serialize optional `texture_guids` sequence. `asset_yaml_test` → **exit 0**.

Log: `.superpowers/sdd/task-12-yaml-green-run.txt`

### RED — dependency graph

Stub `rebuildFromProject` cleared only → **10 failure(s)** (missing Scene→Mesh / Mesh→Texture / leaves).

Log: `.superpowers/sdd/task-12-graph-red-run.txt`

### GREEN — dependency graph

Full rebuild from registry entries + document reads. `asset_dependency_graph_test` → **exit 0** (`asset_dependency_graph_test: all passed`).

Log: `.superpowers/sdd/task-12-graph-green-run.txt`

## Test coverage

| Case | Result |
|------|--------|
| Parse/serialize mesh `texture_guids` | pass |
| Omit empty `texture_guids` on serialize | pass |
| Scene GUID mesh → `dependentsOf(mesh)` includes scene | pass |
| Mesh `texture_guids` → `dependentsOf(tex)` includes mesh | pass |
| Intermediate leaves for mesh/texture/scene | pass |
| Rebuild clears removed scene edge | pass |
| Legacy path mesh ref does not create edge | pass |

## OpenSpec tasks

- [x] 4.1 in `openspec/changes/asset-pipeline-pull/tasks.md`

## Concerns / notes

1. Legacy path mesh refs in scenes are ignored for graph edges (GUID-only); migration still belongs to SceneSerializer load/save.
2. Graph is rebuild-from-project only — not yet wired into watch / cook invalidation (4.2+).
3. `texture_guids` is optional and empty until Import/Cook writers populate it.
4. Runtime-linked tests need `slint_cpp.dll` / `slang.dll` on `PATH`.
5. Did not push.
