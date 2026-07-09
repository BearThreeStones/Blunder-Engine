## Why

Translate handle dragging feels sticky and un-Blender-like: motion uses ray–axis closest-point math (finite plane quads drop tracking), and drag feedback (cursor, constraint guides, ghost, origin dot, handle visibility, outline) does not match Blender’s translate modal UX. Users need screen-space motion and clear drag feedback before we invest in grab (`G`) and full constraint shortcuts.

## What Changes

- Introduce a shared **Translate Modal Session** architecture (P1 wires the **handle** entry only).
- Replace translate-handle motion with Blender-style screen-delta → axis / infinite-plane / free-center projection (including view-aligned axis fallback).
- Add P1 drag feedback: four-way cursor while dragging, constraint guides pinned at drag-start, active-handle ghost at start pose, reference axis arrows following the object, origin-dot coloring, handle show/hide rules, and a fixed light transform-active outline.
- Handle confirm/cancel: release LMB confirms; RMB / Escape cancels and restores the drag-start pose.
- **Out of P1:** grab (`G`) modal, keyboard constraints (X/Y/Z, Shift-plane, re-press space toggle), middle-mouse axis pick, numeric input, rotate/scale modal parity (planned P2/P3).

## Capabilities

### New Capabilities

- `translate-modal-session`: Shared translate interaction session — motion mapping, constraint guides, drag ghost, origin dot, handle visibility during drag, cursor, and confirm/cancel for the handle entry path (P1). Future grab entry reuses this capability.

### Modified Capabilities

- `transform-gizmo`: Translate-axis / plane / center drag scenarios must require screen-space motion and infinite plane projection (not finite plane-quad tracking).
- `outline-drag-color`: While a translate modal session is active, outline uses a fixed light/off-white **transform-active** color (product choice vs Blender); restore selection orange when the session ends. P1 scopes the color change to translate-handle drag (session active).

## Impact

- `TransformGizmoController` translate drag path → delegates to Translate Modal Session
- New session module under `engine/src/runtime/function/render/gizmo/` (motion + session state)
- Overlay draw: constraint guides, ghost handle, origin dot, visibility filters on `TransformGizmoOverlay`
- Cursor: window/SDL modal cursor set/restore during session
- Outline resolve: transform-active color driven by session active (reuse/extend `outline-drag-color` path)
- Docs: `CONTEXT.md` terms already added; `docs/agents/render-pipeline.md` note for translate drag feedback
- Blender reference: `E:/Dev/Blender` (`transform_gizmo_3d.cc`, `transform_input.cc`, `transform_constraints.cc`, `transform_mode_translate.cc`)
