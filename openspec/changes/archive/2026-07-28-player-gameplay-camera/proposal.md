## Why

Player still accepts Editor Camera orbit / authorship pick and draws from `EditorCamera`, so Edit Mode controls leak into Play and the gameplay view is not a scene Camera. Product rule: Player is Gameplay Input only; view must come from a scene **Camera Component**, with Play blocked when the entry scene has none.

## What Changes

- Gate all Player authorship input (Editor Camera, viewport pick, related shortcuts); keep Gameplay Input + window chrome.
- Add native **Camera Component** (FOV / near / far / Main) on scene entities; serialize + load + thin Inspector authoring + Add Camera.
- Resolve Main Camera (else first valid) for Player view/projection only; Edit Mode keeps Editor Camera.
- Play camera preflight fails (no Player spawn) when entry scene has no Camera.
- Play Pause: no Editor Camera orbit; view stays scene Camera (Tick frozen separately).

## Capabilities

### New Capabilities

- `play-camera`: Scene Camera Component model, resolve Main/first, Player view source, Play camera preflight.

### Modified Capabilities

- `play-player`: Player must not accept authorship input or Editor Camera; renders from resolved scene Camera.
- `play-mode`: Play start requires valid Camera on entry scene; Pause does not unlock Editor Camera orbit.
- `gameplay-input`: Clarify Player input surface remains Gameplay Input only (authorship excluded).

## Impact

- `RenderSystem::onEvent` / `tickVulkan` camera path
- `Scene` / `SceneInstance` / serializer / scene load
- `play_preflight` + `ui_host` / PlaySession start
- Inspector Slint + save path
- Tests: authorship policy, resolve, serializer, preflight
- Glossary already in `CONTEXT.md` (Camera Component, Main Camera, Play camera preflight)
