## Why

Authors need a live “through the lens” check while placing and aiming scene **Camera Component** entities. Hierarchy + Inspector numbers and the **Camera Gizmo** wire are not enough; without a PiP preview they must Align View or enter Play to see what the camera actually images.

## What Changes

- Add **Camera Preview**: a floating Slint panel over the editor viewport (default bottom-right) that shows a live secondary render from a selected Camera Component.
- Show when the selection includes any Camera; preview target = primary if it has a Camera, else the first selected Camera entity.
- Panel chrome: title (entity name), drag, resize, collapse; menu shell with Collapse/Expand only.
- Preview image is a dedicated offscreen RT presented as a second Slint `image` (not blit into `viewport-image`).
- Preview content has **no Editor Overlays**; aspect follows the preview content box size (no new CameraComponent.aspect).
- Same-frame refresh when visible and not collapsed; RT longest edge ≤ 480; stop when collapsed; layout memory is in-process only.
- Player never shows Camera Preview.

## Capabilities

### New Capabilities

- `camera-preview`: Live PiP preview of a selected scene Camera Component in the editor viewport.

### Modified Capabilities

- `editor-overlays`: Clarify that Camera Preview is authorship chrome (editor-only) but is **not** drawn by OverlaySystem into the main offscreen; it is a separate Slint panel + secondary RT.

## Impact

- Secondary offscreen RT + overlay-free forward render path hook in `RenderSystem` / `ForwardRenderPath`
- Slint: `camera_preview_panel.slint`, `editor_window.slint`, `SlintSystem` image present + layout sync
- Selection → preview camera resolve helper + unit tests
- Viewport pointer routing: panel rect blocks pick/orbit (like top-right chrome)
- Glossary: **Camera Preview** in `CONTEXT.md`; ADR for secondary offscreen + second Slint image
- Superpowers plan: `docs/superpowers/plans/2026-07-28-camera-preview.md`
