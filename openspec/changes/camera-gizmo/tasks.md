## 1. Frustum / view-frame geometry helpers (TDD)

- [x] 1.1 Pure functions: view-frame corners from FOV + aspect + display distance; up-triangle verts
- [x] 1.2 Unit tests for aspect/FOV extremes
- [x] 1.3 Commit

## 2. Camera Gizmo draw overlay

- [x] 2.1 `CameraGizmoOverlay` wired into `OverlaySystem` (line/screen path as appropriate)
- [x] 2.2 Draw all scene cameras; muted vs selection color; gated by `editorOverlaysEnabled`
- [x] 2.3 Smoke build editor; commit

## 3. Pick priority + selection

- [ ] 3.1 Hit-test body/frame before mesh pick in `RenderSystem::onEvent` / overlay path
- [ ] 3.2 Click selects Camera entity; Player host ignores
- [ ] 3.3 Commit

## 4. FOV / clip handles + history

- [ ] 4.1 Single-selection handles; live update; seal Document History on release
- [ ] 4.2 Inspector FOV/near/far commits use same Command class where practical
- [ ] 4.3 Multi-select: no handles
- [ ] 4.4 Commit

## 5. Align View to Camera / Align Camera to View

- [ ] 5.1 Target resolve helper (single Camera selection → else Main → else first → fail); unit tests
- [ ] 5.2 Align View: Editor Camera pose + FOV; no history
- [ ] 5.3 Align Camera: write pose + FOV; history Command
- [ ] 5.4 Menu + Numpad0 / Ctrl+Alt+Numpad0 + laptop fallbacks
- [ ] 5.5 Commit

## 6. Docs / QA

- [ ] 6.1 Confirm `CONTEXT.md` Camera Gizmo terms match shipped behavior
- [ ] 6.2 Manual checklist: draw, pick over mesh, FOV/clip undo, Align both ways, Player no gizmo
- [ ] 6.3 OpenSpec task boxes; ready to archive after USER-VERIFY
