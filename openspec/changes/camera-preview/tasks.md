## 1. Domain + scaffolding

- [x] 1.1 Add **Camera Preview** glossary entry to `CONTEXT.md` (near Camera Gizmo / Editor Overlay)
- [x] 1.2 Add ADR `docs/adr/0018-camera-preview-secondary-offscreen.md` (secondary RT + second Slint image)
- [x] 1.3 Branch `feat/camera-preview` if not already on a feature branch

## 2. Resolve + matrices (TDD)

- [ ] 2.1 Add `resolve_camera_preview_target.h` + unit test: primary-if-Camera else first selected Camera; empty → none
- [ ] 2.2 Add `camera_preview_matrices.h` (or reuse single-input path through `resolvePlayCamera`) + test: aspect/FOV/near/far → projection; world → view
- [ ] 2.3 Add `camera_preview_rt_size.h` + test: longest edge clamp ≤480; preserves aspect; zero dims → skip

## 3. Secondary render

- [ ] 3.1 Extend `ForwardRenderPath` with `renderFrameTo(target, frame_state, draws…, draw_overlays)`
- [ ] 3.2 Allocate/resize dedicated preview offscreen in `RenderSystem`; skip when no target / collapsed
- [ ] 3.3 After main frame, build preview `ForwardFrameState` (shadows off, no overlays) and `renderFrameTo`
- [ ] 3.4 CPU readback → `SlintSystem::setCameraPreviewImage`

## 4. Slint chrome

- [ ] 4.1 Create `camera_preview_panel.slint` (title, menu Collapse, drag, resize, collapse, image)
- [ ] 4.2 Wire into viewport tile in `editor_window.slint` + `slint_target_sources` in `runtime/CMakeLists.txt`
- [ ] 4.3 Sync visibility, title, image, layout props from `SlintSystem`

## 5. Input hit block

- [ ] 5.1 Publish panel rect (viewport-local) to C++; add `hitCameraPreviewPanelLocal`
- [ ] 5.2 Gate mesh pick / Editor Camera orbit when pointer is over the panel (incl. collapsed title bar)

## 6. Validate

- [ ] 6.1 Run unit tests: resolve, matrices, RT size
- [ ] 6.2 Build `engine_editor`; manual USER-VERIFY checklist in OpenSpec / plan
- [ ] 6.3 Confirm `engine_player` has no Camera Preview UI
