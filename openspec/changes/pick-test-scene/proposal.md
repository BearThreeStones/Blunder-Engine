## Why

Manual QA for hybrid GPU picking (piercing menu, same-pixel cycling, parent promotion) is unreliable on the default `root.scene.asset` because Sponza and nested sub-scenes add thousands of overlapping surfaces and deep hierarchy. We need a minimal, deterministic scene with a few axis-aligned cubes so engineers can reproduce multi-hit pick behavior in seconds.

## What Changes

- Add a **glTF 2.0 cube** under `Resources/Models/PickTest/` and a matching `Assets/Meshes/Cube.mesh.yaml` descriptor.
- Add **`assets/Scenes/pick_test.scene.asset`** with three **root-level** box entities (`BoxFront`, `BoxMid`, `BoxBack`) sharing the cube mesh, positioned along the view axis so they overlap at the default editor camera.
- Strip pick-test noise from the default scene: remove `Sponza.mesh` and `SubLevel` child scene from `root.scene.asset` (keep `Sun` / `CameraRig` or equivalent camera rig only).
- Point editor startup at `pick_test.scene.asset` (or document `BLUNDER_STARTUP_SCENE` override if we add one).
- Add a short **QA checklist** in agent docs linking scene layout to expected pick outcomes (3 menu rows, 3-cycle stack, no parent dedupe on root boxes).

## Capabilities

### New Capabilities

- `pick-test-scene`: Minimal authored scene and cube mesh assets for viewport pick regression testing.

### Modified Capabilities

<!-- No existing openspec/specs capability requirements change; this is content + startup wiring only. -->

## Impact

- **Assets:** `Resources/Models/PickTest/`, `Assets/Meshes/Cube.mesh.yaml`, `Assets/Scenes/pick_test.scene.asset`, `Assets/Scenes/root.scene.asset`.
- **Runtime:** `engine.cpp` startup scene path (or env override).
- **Registry:** new GUID entry in `.blunder/asset_registry.yaml` after cook (generated, not hand-edited).
- **Docs:** `docs/agents/` pick QA note cross-referencing `hybrid-gpu-picking` manual tasks.
- **No** pick-system code changes in this change unless startup env hook is added.
