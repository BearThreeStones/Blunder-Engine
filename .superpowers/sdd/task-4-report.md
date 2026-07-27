# Task 4 Report — SceneInstance camera storage + load attach

**Status:** DONE  
**Date:** 2026-07-27  
**Branch:** feat/player-gameplay-camera

## Built

| API | Location |
|-----|----------|
| `setCamera` / `getCamera` / `forEachCamera` | `scene_instance.h/.cpp` — `m_cameras` map mirrors mesh renderer storage |
| `resolvePlayCameraFromScene` | `scene_instance.h` (inline; needs `SceneInstance` + EASTL) |
| `attachSceneEntityCameras` | `scene_system.cpp` — called from `instantiateScene` after mesh attach |

## Load path

On scene instantiate, `attachSceneEntityCameras` iterates `SceneEntityDefinition` entries with `has_camera`, finds entity by name, calls `setCamera` with deserialized `definition.camera`.

## Tests

| Target | Result |
|--------|--------|
| `engine_runtime` (Debug) | PASS |
| `play_camera_resolve_test` | PASS (existing tests; no new scene-instance unit test) |

## Commit

`feat(scene): attach CameraComponent on scene load` — files: `scene_instance.h/.cpp`, `scene_system.h/.cpp`

## Concerns

- `resolvePlayCameraFromScene` lives in `scene_instance.h` (not `play_camera_resolve.h`) to avoid EASTL include path issues in lightweight test targets.
- Scene reload path only re-attaches meshes via `needsMeshAttach`; cameras attach on fresh instantiate only (same as initial load).
- `exportToScene` does not yet round-trip instance cameras back to definitions (future save-path work).

## Review fix (2026-07-27)

Commit `67a5ec9` accidentally added mesh-asset loading (`.mesh.yaml` / `.mesh.asset` / `loadMesh` + `mesh_asset.h`) inside `attachSceneEntityMeshes`. That was out of Task 4 scope.

**Fix:** Restored `attachSceneEntityMeshes` to GltfSceneImporter-only path (as at parent `8ef02bd`); kept `attachSceneEntityCameras` call + CameraComponent APIs.

| Target | Result |
|--------|--------|
| `engine_runtime` (Debug) | PASS |

**Commit:** `8e2aed0` — `fix(scene): drop unrelated mesh-asset load from camera attach commit`
