## 1. Player authorship input policy (TDD)

- [x] 1.1 Add failing `player_authorship_input_test` for Editor vs Player
- [x] 1.2 Add `player_authorship_input.h` and make the test pass
- [x] 1.3 Register CTest target; gate `RenderSystem::onEvent` (EditorCamera + pick); lock interaction in Player initialize
- [x] 1.4 Build `engine_player`; commit

## 2. CameraComponent + resolvePlayCamera (TDD)

- [x] 2.1 Add failing `play_camera_resolve_test` (empty / one / Main / first / FOV)
- [x] 2.2 Add `camera_component.h` + `play_camera_resolve.h`; make tests pass
- [x] 2.3 Register CTest; commit

## 3. Scene definition + serializer

- [x] 3.1 Extend serializer tests for `"camera"` round-trip and absent key
- [x] 3.2 Add `has_camera` / `camera` on `SceneEntityDefinition`; parse/write JSON
- [x] 3.3 Run `scene_serializer_test`; commit

## 4. SceneInstance storage + load attach

- [x] 4.1 `setCamera` / `getCamera` / `forEachCamera` on `SceneInstance`
- [x] 4.2 Attach on load when `has_camera`; add `resolvePlayCameraFromScene`
- [x] 4.3 Build smoke; commit

## 5. RenderSystem Player uses resolved Camera

- [x] 5.1 Branch `tickVulkan` Player path to `resolvePlayCameraFromScene`; skip EditorCamera update in Player
- [x] 5.2 Build `engine_player` + `engine_editor`; commit

## 6. Play camera preflight

- [x] 6.1 Unit tests for `sceneAssetHasPlayCamera` / `runPlayCameraGate`
- [x] 6.2 Implement gate helpers
- [x] 6.3 Call before Player spawn in Play start; commit

## 7. Inspector thin Camera authoring + Add

- [x] 7.1 Ensure save path emits `"camera"` from instance
- [x] 7.2 Slint FOV/near/far/Main + Add Camera
- [x] 7.3 Manual stickiness check; commit

## 8. Content seed + dual-window verification

- [x] 8.1 Seed Main Camera on Play entry Test scene
- [x] 8.2 Manual checklist (Edit orbit / preflight fail / Player no orbit / Pause / editor while Playing)
- [x] 8.3 Commit content
