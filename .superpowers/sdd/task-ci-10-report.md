# Task 4.1 Report — Smoke / related tests for COLLADA Intermediate

**Status:** DONE  
**Branch:** `feat/asset-pipeline-pull`  
**Worktree:** `e:\Dev\Blunder-Engine\.worktrees\asset-pipeline-pull`  
**Commits:** `83f9886` (fixtures + mark 4.1), `dcd5f10` (report)

## Summary

Updated remaining new-Asset Intermediate fixtures from `.gltf` → `.dae` and tightened smoke assertions that Intermediate `source` is COLLADA (not glTF/GLB). Legacy glTF retained only for upgrade / Fast Path legacy scenarios in `asset_import_test` / `asset_manager_fast_path_test`.

| File | Change |
|------|--------|
| `asset_pipeline_smoke_test.cpp` | Assert Intermediate `.dae`; label Intermediate COLLADA exists |
| `asset_manager_guid_test.cpp` | Minimal COLLADA triangle fixture; `source` → `.dae` |
| `asset_dependency_graph_test.cpp` | Mesh Intermediate leaves → `.dae` |
| `asset_yaml_test.cpp` | Descriptor parse/serialize fixtures → `.dae` |
| `scene_serializer_test.cpp` | Legacy-path migration mesh `source` → `.dae` |
| `openspec/.../tasks.md` | `[x] 4.1` |

No production code changes (fixture/assertion only).

## Evidence

Build: `cmake --build build/vs2026-debug --config Debug` for smoke/guid/dependency/yaml/scene_serializer targets → succeeded

| Test | Result |
|------|--------|
| `asset_pipeline_smoke_test` | all passed (OBJ → `.dae` Intermediate) |
| `asset_manager_guid_test` | all passed (loads `tri.dae`) |
| `asset_dependency_graph_test` | all passed |
| `asset_yaml_test` | all passed (exit 0) |
| `scene_serializer_test` | all passed |

## Concerns

- Sample project Assets may still point at legacy `.gltf` Intermediate until Intermediate Upgrade / Task 4.3
- Full targeted suite still owned by task 4.2
- Did not push
