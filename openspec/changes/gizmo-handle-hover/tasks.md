## 1. Pick helpers + unit test

- [x] 1.1 Add `gizmoHandleHighlighted(active, hover, axis)` to `transform_gizmo_pick.h/.cpp`
- [x] 1.2 Add `pickTransformGizmoHandle(mode, ctx)` mode-dispatching wrapper
- [x] 1.3 Create `engine/src/tests/transform_gizmo_hover_test.cpp` (highlight precedence + pick wrapper)
- [x] 1.4 Register test in `engine/src/tests/CMakeLists.txt`; verify pass

## 2. Controller hover state

- [x] 2.1 Add `m_hover_axis`, `updateHoverFromPointer`, `clearHover`, `isHandleHighlighted`, `hasHover` to `TransformGizmoController`
- [x] 2.2 Extract `buildPickContext(window_pos, camera, ...)` from `onMousePressed`; reuse in hover and press
- [x] 2.3 Clear hover on `setMode`, `cancelInteraction`, and drag start (`onMousePressed` hit)
- [x] 2.4 Refactor `onMousePressed` to use `pickTransformGizmoHandle`

## 3. Overlay highlight

- [x] 3.1 Replace drag-only `highlight` checks in `recordGizmoDraw` with `m_controller.isHandleHighlighted(axis)`
- [x] 3.2 Keep rotation ghost dial branch (drag-only) unchanged

## 4. Input wiring

- [x] 4.1 Wire `RenderSystem::onEvent` `MouseMoved` → `updateHoverFromPointer`; `requestViewportRedraw()` on change only
- [x] 4.2 Clear hover when pointer leaves viewport or gizmo mode is `none`
- [x] 4.3 Confirm hover path does **not** set `event.handled`

## 5. Docs and validation

- [x] 5.1 Add hover paragraph to `docs/agents/render-pipeline.md` (input priority section)
- [x] 5.2 Build `engine_editor` Debug
- [ ] 5.3 Manual: G/R/S — hover each interactive handle type; color_hi visible
- [ ] 5.4 Manual: drag handle — hover suppressed, active stays highlighted
- [ ] 5.5 Manual: click mesh behind gizmo — selection still works; gizmo handle click unchanged
- [x] 5.6 `openspec validate gizmo-handle-hover`
