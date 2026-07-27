## Why

Play Mode’s Player window currently draws and hit-tests Editor Overlays (ground grid, Transform gizmo, Navigate gizmo, and related authorship chrome). That leaks Edit Mode tools into gameplay view and fights the product rule that Player is not an authorship UI.

## What Changes

- Disable Editor Overlay **draw** in `EngineHostMode::Player`.
- Disable Editor Overlay **interaction** (Transform / Navigate gizmo input) in Player.
- Keep editor viewport overlays unchanged while a Play Session runs.
- Play Pause still hides overlays (Pause ≠ Edit Mode).
- No Player env override to force overlays on in this slice.
- Editor Camera orbit of the Player view remains.

## Capabilities

### New Capabilities

- `editor-overlays`: When authorship viewport chrome (Editor Overlays) may draw and accept input, keyed by engine host mode.

### Modified Capabilities

- `play-player`: Player must not show or interact with Editor Overlays.

## Impact

- `OverlaySystem` begin_sync / draw paths
- `RenderSystem::onEvent` Transform / Navigate handling
- New policy header + unit test
- Glossary: `CONTEXT.md` **Editor Overlay** (already defined)
