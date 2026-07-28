## Why

Authors need to see and manipulate scene **Camera Component** entities in the editor viewport the way Blender does — wire body, view frame, FOV/clip handles, and one-shot Align View/Camera — instead of relying only on Hierarchy + Inspector numbers.

## What Changes

- Add **Camera Gizmo** Editor Overlay (Blender-like wire: origin, frustum edges, view frame, up triangle).
- Draw all cameras (muted when unselected); single-selection shows FOV/near/far handles.
- Hit-test Camera Gizmo before mesh pick; reuse Transform gizmo for pose.
- Drag FOV / clip with Document History seal on release; Inspector camera commits share the same Command class.
- **Align View to Camera** / **Align Camera to View** (menu + Numpad0 / Ctrl+Alt+Numpad0 + laptop fallbacks); target = single Camera selection, else Main then first; multi-select invalid.
- Player never draws or interacts with Camera Gizmo (existing Editor Overlay host gate).

## Capabilities

### New Capabilities

- `camera-gizmo`: Camera Component visualization and interaction in the editor viewport, including Align commands.

### Modified Capabilities

- `editor-overlays`: Camera Gizmo is part of authorship chrome gated by Editor host.
- `play-camera`: No requirement change to Play resolve; Align unselected target uses the same Main-then-first rule.
- `document-history` / scene-edit commands (if present): Camera parameter and Align Camera to View Commands — only if those specs exist; otherwise cover under `camera-gizmo`.

## Impact

- New overlay under `engine/src/runtime/function/render/overlay/` (+ optional line-pass geometry)
- `OverlaySystem` + `RenderSystem::onEvent` hit order
- Editor menu / shortcut wiring (`slint_system` / UI host)
- Document History Commands for FOV/clip and Align Camera to View
- Tests: frustum geometry, hit priority, resolve target for Align, history seal
- Glossary already updated in `CONTEXT.md`
