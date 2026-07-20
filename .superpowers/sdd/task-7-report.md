# Task 7 Report — OpenSpec 2.4 GUID load/resolve helpers

**Status:** done  
**Branch:** `feat/asset-pipeline-pull`  
**Worktree:** `e:\Dev\Blunder-Engine\.worktrees\asset-pipeline-pull`  
**Commit:** (see git log after commit)

## Summary

`AssetManager` now exposes thin GUID wrappers: `resolveGuidPath`, `loadMeshByGuid`, `loadTexture2DByGuid`, and `loadSceneByGuid`. Each resolves the GUID through `AssetRegistry` then delegates to the existing path-based `load*`. Path-based APIs are unchanged. `SceneSystem::attachSceneEntityMeshes` uses `resolveGuidPath` instead of calling the registry ad-hoc.

## TDD evidence

### RED (feature missing)

`asset_manager_guid_test` called `loadMeshByGuid` / `resolveGuidPath` before those methods existed.

Build failed with MSVC `C2039`, e.g.:

```
asset_manager_guid_test.cpp: error C2039: "loadMeshByGuid": not a member of "Blunder::AssetManager"
asset_manager_guid_test.cpp: error C2039: "resolveGuidPath": not a member of "Blunder::AssetManager"
```

Full log: `.superpowers/sdd/task-7-red-build.txt`  
Excerpt: `.superpowers/sdd/task-7-red-excerpt.txt`

### GREEN (implementation)

Minimal production changes:

- `asset_manager.h` — declare GUID resolve/load helpers + `AssetRegistry` forward decl
- `asset_manager.cpp` — thin wrappers (resolve → path `load*`)
- `scene_system.cpp` — GUID→path bridge uses `resolveGuidPath`
- `engine/src/tests/asset_manager_guid_test.cpp` + CMake wiring (`cgltf` include)

Build: `cmake --build build/vs2026-debug --config Debug --target asset_manager_guid_test` → succeeded  
Run: `asset_manager_guid_test.exe` → **exit 0** (`asset_manager_guid_test: all passed`)

Logs: `.superpowers/sdd/task-7-green-build.txt`, `.superpowers/sdd/task-7-green-run.txt`

## Test coverage

| Case | Result |
|------|--------|
| `resolveGuidPath` returns registered mesh descriptor path | pass |
| `loadMeshByGuid` loads mesh from temp project descriptor + embedded glTF | pass |
| GUID + path load share cached `MeshAsset` identity | pass |
| Unknown GUID → empty path / nullptr mesh | pass |

## OpenSpec tasks

- [x] 2.4 in `openspec/changes/asset-pipeline-pull/tasks.md`

## Concerns / notes

1. `GltfSceneImporter` still needs a descriptor/glTF **path**, so SceneSystem resolves GUID→path then imports; it does not call `loadMeshByGuid` (that loads a single `MeshAsset`, not a full glTF hierarchy).
2. Descriptor→source Fast Path still does not cache under the `.mesh.yaml` key when cooked is missing (pre-existing); GUID and path loads still share identity via the Intermediate glTF cache key.
3. Texture/scene GUID load helpers are implemented symmetrically but only mesh load-by-guid is covered by the new unit test (2.5 may expand).
4. Runtime-linked tests need `slint_cpp.dll` on `PATH`.
5. Did not push.
