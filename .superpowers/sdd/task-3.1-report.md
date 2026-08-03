# Task 3.1 Report — Scene-embedded AnimationTree topology round-trip

**Status:** DONE  
**Branch:** `feat/dogwalk-animation-phase-4`

## Summary

AnimationTree topology (BlendSpace1D points/scalars, StateMachine states, Add2 clip+weight, OneShot slot, active flag, current state, base blend-space node) now persists on scene entities under `animationTree`, following the `animationPlayer` embed pattern. `SceneInstance` applies topology on instantiate and captures it on `exportToScene`.

## JSON shape (entity `animationTree`)

```json
"animationTree": {
  "active": true,
  "currentState": "Locomotion",
  "baseBlendSpaceNode": "Locomotion",
  "blendSpaces": {
    "Locomotion": {
      "scalar": 1.0,
      "points": [
        { "clip": "idle", "scalar": 0.0 },
        { "clip": "walk", "scalar": 1.0 }
      ]
    }
  },
  "states": {
    "Locomotion": { "kind": "blendSpace1D", "blendSpaceNode": "Locomotion" }
  },
  "add2": { "clip": "turn", "weight": 0.4 },
  "oneShotClip": "trip"
}
```

OneShot live playback state is **not** serialized (authored slot only).

## Production changes

| File | Change |
|------|--------|
| `scene.h` | `AnimationTreeBlendSpaceDef`, `AnimationTreeStateDef`, entity topology fields |
| `scene_serializer.cpp` | `appendAnimationTreeJson`, `parseAnimationTreeObject` (+ helpers) |
| `scene_instance.cpp` | `applyAnimationTreeTopology`, `captureAnimationTreeTopology`, instantiate/export |
| `animation_tree.h/.cpp` | `setOneShotSlotClip`, `visitBlendSpaces`, `visitStates` |

## Tests

| Target | Coverage |
|--------|----------|
| `scene_serializer_test.cpp` | `serializeAndParseAnimationTreeTopology` |
| `scene_behaviour_instantiate_test.cpp` | `instantiateRestoresAnimationTreeTopology` (+ export) |

**Note:** `scene_serializer_test` / `scene_behaviour_instantiate_test` link `blunder_engine_c_static` (~43MB) and failed to launch in this worktree (missing runtime DLL chain at test cwd). They **compile** after the serializer changes. `animation_tree_test` (571KB) passes including expanded ClassDB named-API coverage.

## Test command

```powershell
cmake --build build/vs2026-debug --target animation_tree_test scene_serializer_test scene_behaviour_instantiate_test --config Debug
# Lightweight (verified):
build/vs2026-debug/engine/src/tests/Debug/animation_tree_test.exe
# Heavy scene tests need VulkanSDK Bin + slint_cpp.dll on PATH (see docs/agents/testing.md)
```

## Concerns

- Instantiate apply order: clip map must be bound on AnimationPlayer before tree topology (player ensured when tree or clips present).
- `setStateBlendSpace` / `addBlendSpacePoint` order matters at runtime; scene JSON lists blend spaces before states in apply loop.
