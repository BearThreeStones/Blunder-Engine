## 1. Player authorship input policy (TDD)

- [ ] 1.1 Add failing `player_authorship_input_test` for Editor vs Player
- [ ] 1.2 Add `player_authorship_input.h` and make the test pass
- [ ] 1.3 Register CTest target; gate `RenderSystem::onEvent` (EditorCamera + pick); lock interaction in Player initialize
- [ ] 1.4 Build `engine_player`; commit

## 2. CameraComponent + resolvePlayCamera (TDD)

- [ ] 2.1 Add failing `play_camera_resolve_test` (empty / one / Main / first / FOV)
- [ ] 2.2 Add `camera_component.h` + `play_camera_resolve.h`; make tests pass
- [ ] 2.3 Register CTest; commit

## 3. Scene definition + serializer

- [ ] 3.1 Extend serializer tests for `"camera"` round-trip and absent key
- [ ] 3.2 Add `has_camera` / `camera` on `SceneEntityDefinition`; parse/write JSON
- [ ] 3.3 Run `scene_serializer_test`; commit

## 4. SceneInstance storage + load attach

- [ ] 4.1 `setCamera` / `getCamera` / `forEachCamera` on `SceneInstance`
- [ ] 4.2 Attach on load when `has_camera`; add `resolvePlayCameraFromScene`
- [ ] 4.3 Build smoke; commit

## 5. RenderSystem Player uses resolved Camera

- [ ] 5.1 Branch `tickVulkan` Player path to `resolvePlayCameraFromScene`; skip EditorCamera update in Player
- [ ] 5.2 Build `engine_player` + `engine_editor`; commit

## 6. Play camera preflight

- [ ] 6.1 Unit tests for `sceneAssetHasPlayCamera` / `runPlayCameraGate`
- [ ] 6.2 Implement gate helpers
- [ ] 6.3 Call before Player spawn in Play start; commit

## 7. Inspector thin Camera authoring + Add

- [ ] 7.1 Ensure save path emits `"camera"` from instance
- [ ] 7.2 Slint FOV/near/far/Main + Add Camera
- [ ] 7.3 Manual stickiness check; commit

## 8. Content seed + dual-window verification

- [ ] 8.1 Seed Main Camera on Play entry Test scene
- [ ] 8.2 Manual checklist (Edit orbit / preflight fail / Player no orbit / Pause / editor while Playing)
- [ ] 8.3 Commit content
