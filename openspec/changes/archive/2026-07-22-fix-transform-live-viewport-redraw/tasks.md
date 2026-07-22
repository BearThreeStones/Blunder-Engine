## 1. TDD notify helpers

- [x] 1.1 Add `transform_edit_viewport_notify.{h,cpp}` with `notifyViewportAfterGizmoTransformEdit` and `notifyViewportAfterInspectorTransformEdit`
- [x] 1.2 Add `engine/src/tests/transform_edit_viewport_notify_test.cpp` (generation bump / null no-op cases) and register it in `engine/src/tests/CMakeLists.txt`
- [x] 1.3 Wire the new `.cpp` into the runtime CMake target that compiles sibling render sources
- [ ] 1.4 Run RED then GREEN: build + `ctest -R transform_edit_viewport_notify_test` (not rerun in Task 4)

## 2. Call-site wiring

- [x] 2.1 Call `notifyViewportAfterGizmoTransformEdit` from gizmo anonymous `markSceneDirty()` in `transform_gizmo_controller.cpp`
- [x] 2.2 Call `notifyViewportAfterInspectorTransformEdit` from `SlintSystem::applyInspectorTransform`
- [ ] 2.3 Build `engine_runtime` and re-run `transform_edit_viewport_notify_test`

## 3. Docs and validation

- [x] 3.1 Document transform-edit → redraw next to selection present in `docs/agents/render-pipeline.md`
- [ ] 3.2 Manual QA pending human verification:
  - [ ] Select mesh; translate drag under static camera — mesh and gizmo move
  - [ ] Release — pose sticks
  - [ ] Inspector position edit — updates without camera move
  - [ ] Rotate/scale drag
  - [ ] Escape cancel restores pose
  - [ ] Orbit/selection outline sanity
