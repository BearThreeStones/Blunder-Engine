# Task 6 Report — OpenSpec 2.3 Scene serializer GUID + mesh Asset References

**Status:** done  
**Branch:** `feat/asset-pipeline-pull`  
**Worktree:** `e:\Dev\Blunder-Engine\.worktrees\asset-pipeline-pull`  
**Commit:** `69b9b08` — feat: scene serializer GUID and mesh Asset References

## Summary

`SceneSerializer` now reads/writes top-level `"guid"` via `Scene::getGuid`/`setGuid`. Entity `"mesh"` persists a Mesh Asset GUID when possible; legacy `assets/...mesh.yaml` paths migrate through `AssetRegistry::findGuidForPath` on load (and rewrite on save) when a registry is supplied. Editor/AssetManager load/save paths call `ensureSceneAssetRegistered` and pass the registry into serialize/deserialize. `SceneSystem` resolves GUID mesh refs to paths until 2.4 GUID load APIs land.

## TDD evidence

### RED (feature missing)

`scene_serializer_test` expected `Scene::setGuid`/`getGuid` and 3-arg `SceneSerializer::{serialize,deserialize}` before those APIs existed.

Build failed with MSVC `C2039` / `C2660`, e.g.:

```
scene_serializer_test.cpp: error C2039: "setGuid": not a member of "Blunder::Scene"
scene_serializer_test.cpp: error C2660: SceneSerializer::deserialize does not take 3 arguments
scene_serializer_test.cpp: error C2660: SceneSerializer::serialize does not take 3 arguments
```

Full log: `.superpowers/sdd/task-6-red-build.txt`  
Excerpt: `.superpowers/sdd/task-6-red-excerpt.txt`

### GREEN (implementation)

Minimal production changes:

- `scene.h` — `m_guid` + accessors; document dual meaning of `mesh_virtual_path`
- `scene_serializer.h/.cpp` — optional `AssetRegistry*`; guid R/W; path→GUID migrate/rewrite
- `asset_manager.cpp` — `ensureSceneAssetRegistered` + deserialize with registry
- `editor_scene_edit_system.cpp` — ensure on open/save; serialize with registry; copy guid when saving
- `scene_system.cpp` — GUID→path bridge for mesh attach (temporary until 2.4)
- `engine/src/tests/scene_serializer_test.cpp` + CMake wiring

Build: `cmake --build build/vs2026-debug --config Debug --target scene_serializer_test` → succeeded  
Run: `scene_serializer_test.exe` → **exit 0** (`scene_serializer_test: all passed`)

Logs: `.superpowers/sdd/task-6-green-build.txt`, `.superpowers/sdd/task-6-green-run.txt`  
Re-verify: `.superpowers/sdd/task-6-verify-green-build.txt`, `.superpowers/sdd/task-6-verify-green-run.txt`

## Test coverage

| Case | Result |
|------|--------|
| Serialize/parse top-level scene `guid` | pass |
| Save entity `mesh` that is already a GUID | pass |
| Load legacy path mesh + registry → in-memory GUID; save emits GUID not path | pass |

## OpenSpec tasks

- [x] 2.3 in `openspec/changes/asset-pipeline-pull/tasks.md`

## Concerns / notes

1. `mesh_virtual_path` keeps its field name but dual-means GUID (preferred) or legacy path until migration; callers must treat GUID as first-class after load-with-registry.
2. `SceneSystem` GUID→path resolve is a temporary bridge for mesh import until 2.4 GUID load helpers exist.
3. Without a registry pointer, legacy mesh paths are left unchanged (no silent migration).
4. Runtime-linked tests need `slint_cpp.dll` (and typically `slang.dll`) on `PATH`.
5. Did not implement 2.4 GUID-based AssetManager load APIs. Did not push.

---

## Review fix — exportToScene preserves mesh Asset Reference

**Finding:** `SceneInstance::exportToScene` never copied `mesh_virtual_path`, so editor `saveActiveScene` dropped mesh GUID/path refs even though SceneSerializer was correct.

**Fix commit:** (pending below) — store mesh ref on `Entity` through instantiate/export; spawn sets it too.

### RED

`scene_soft_delete_test` cases `exportPreservesMeshGuidReference` / `exportPreservesMeshPathReference`:

```
FAIL export preserves mesh guid
FAIL export preserves mesh path
2 failure(s)
```

Log: `.superpowers/sdd/task-6-fix-red-run.txt`

### GREEN

- `entity.h` — `getMeshVirtualPath` / `setMeshVirtualPath`
- `scene_instance.cpp` — copy mesh ref on `instantiate` and `exportToScene`
- `editor_scene_edit_system.cpp` — `spawnMeshAsset` stamps mesh Asset Reference onto the entity
- `scene_soft_delete_test.cpp` — round-trip coverage

Build/run: `scene_soft_delete_test` → exit 0; `scene_serializer_test` → exit 0  
Logs: `.superpowers/sdd/task-6-fix-green-build.txt`, `.superpowers/sdd/task-6-fix-green-run.txt`
