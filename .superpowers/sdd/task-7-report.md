# Task 7 Report — OpenSpec 2.4 GUID load/resolve helpers

**Status:** done  
**Branch:** `feat/asset-pipeline-pull`  
**Worktree:** `e:\Dev\Blunder-Engine\.worktrees\asset-pipeline-pull`  
**Commit:** `a9f371c` — feat: AssetManager GUID load/resolve helpers

## Summary

`AssetManager` now exposes thin GUID helpers that resolve via `AssetRegistry::resolveGuid` then call existing path-based `load*`:

- `resolveGuidPath(guid, registry)`
- `loadMeshByGuid` / `loadTexture2DByGuid` / `loadSceneByGuid`

Path-based APIs are unchanged. `SceneSystem::attachSceneEntityMeshes` uses `resolveGuidPath` instead of calling the registry directly.

## TDD evidence

### RED (feature missing)

`asset_manager_guid_test` called `loadMeshByGuid` / `resolveGuidPath` before definitions were linked into `engine_runtime`.

Link failed with MSVC `LNK2019` (unresolved externals), e.g.:

```
asset_manager_guid_test.obj : error LNK2019: unresolved external symbol
  AssetManager::resolveGuidPath(...)
asset_manager_guid_test.obj : error LNK2019: unresolved external symbol
  AssetManager::loadMeshByGuid(...)
```

Full log: `.superpowers/sdd/task-7-red-build.txt` (from `task-7-red-out2.txt`)  
Excerpt: `.superpowers/sdd/task-7-red-excerpt.txt`

### GREEN (implementation)

Minimal production changes:

- `asset_manager.h` — declare resolve + load-by-GUID helpers; forward-declare `AssetRegistry`
- `asset_manager.cpp` — resolve then delegate to `loadMesh` / `loadTexture2D` / `loadScene`
- `scene_system.cpp` — GUID→path bridge uses `AssetManager::resolveGuidPath`
- `engine/src/tests/asset_manager_guid_test.cpp` + CMake wiring (`cgltf` include)

Build: `cmake --build build/vs2026-debug --config Debug --target asset_manager_guid_test` → succeeded  
Run: `asset_manager_guid_test.exe` → **exit 0** (`asset_manager_guid_test: all passed`)

Logs: `.superpowers/sdd/task-7-green-build.txt`, `.superpowers/sdd/task-7-green-run.txt`

## Test coverage

| Case | Result |
|------|--------|
| `loadMeshByGuid` loads registered `.mesh.yaml` via temp project (Fast Path Intermediate glTF) | pass |
| Vertex/index counts match path-based load | pass |
| `resolveGuidPath` returns descriptor virtual path | pass |
| Unknown GUID returns nullptr | pass |

## OpenSpec tasks

- [x] 2.4 in `openspec/changes/asset-pipeline-pull/tasks.md`

## Concerns / notes

1. Descriptor Fast Path without cooked Final still falls back to Intermediate glTF (expected until 3.x); test asserts load success, not Final preference.
2. `loadMesh` descriptor path does not cache under the descriptor key on Intermediate fallback (pre-existing); GUID helpers inherit that. Test compares geometry counts, not pointer identity.
3. Texture/scene GUID load helpers are thin wrappers covered by the same resolve pattern; mesh is the exercised path without Vulkan.
4. Runtime-linked tests need `slang.dll` / `slint_cpp.dll` on `PATH`.
5. Did not push.
