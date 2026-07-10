## 1. Cube mesh asset

- [x] 1.1 Copy `Cube.gltf` and required buffer files into `Resources/Models/PickTest/`
- [x] 1.2 Add `Assets/Meshes/Cube.mesh.yaml` with new GUID and `source: resources/Models/PickTest/Cube.gltf`
- [x] 1.3 Run asset cook (editor startup or `asset_compiler`) and verify mesh loads

## 2. Pick-test scene

- [x] 2.1 Create `Assets/Scenes/pick_test.scene.asset` with `BoxFront`, `BoxMid`, `BoxBack` (root entities, shared cube mesh, Y-stacked positions)
- [x] 2.2 Trim `Assets/Scenes/root.scene.asset`: remove `Sponza.mesh` and `childScenes` (keep `Sun` / `CameraRig`)

## 3. Editor startup

- [x] 3.1 Set default `k_startup_scene_path` to `assets/Scenes/pick_test.scene.asset`
- [x] 3.2 Add `BLUNDER_STARTUP_SCENE` env override in `engine.cpp` (optional path to another `.scene.asset`)

## 4. Documentation

- [x] 4.1 Add pick-test QA section to `docs/agents/render-pipeline.md` or `docs/agents/testing.md` (scene path, entity names, expected ≥3 multi-hit rows)

## 5. Validation

- [x] 5.1 Build `engine_editor`; confirm hierarchy shows three box entities
- [ ] 5.2 Frame scene in viewport; verify overlapping pick pixel (manual: Ctrl+right menu / click cycle when hybrid pick multi-hit works)
