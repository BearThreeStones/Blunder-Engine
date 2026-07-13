## 1. Transform ops (TDD)

- [x] 1.1 Add `inspector_transform_ops.h` / `.cpp` with scale-link, mixed detect, Absolute/Delta component apply, Euler delta, quat normalize
- [x] 1.2 Register sources in `engine/src/runtime/CMakeLists.txt`
- [x] 1.3 Add `engine/src/tests/inspector_transform_ops_test.cpp` and register in `engine/src/tests/CMakeLists.txt`
- [x] 1.4 Build and run `inspector_transform_ops_test` until green

## 2. Slint Transform UI

- [x] 2.1 Create `axis_number_field.slint` (axis color, unit, LineEdit commit, scrub, optional under-slider)
- [x] 2.2 Redesign Transform section in `inspector_panel.slint` (collapse, Position/Rotation/Scale rows, Scale link, Rotation Edit Mode, Multi-edit Absolute/Delta, mixed flags, quat fields)
- [x] 2.3 Plumb new properties/callbacks through `editor_window.slint` (both Inspector embed sites) and floating panel bindings if needed
- [x] 2.4 Build `engine_editor` and fix Slint fork compile issues for focus/scrub APIs

## 3. Sync / apply wiring

- [x] 3.1 Add session state + focused-field id on `SlintSystem` (rotation mode, scale link, multi-edit mode)
- [x] 3.2 Rewrite `syncInspectorFromSelection` for multi-select, mixed/Delta display, focus lock, Euler|Quaternion push
- [x] 3.3 Rewrite `applyInspectorTransform` for Absolute/Delta, Scale link, Euler/Quat single-select, Euler-delta multi rotation, viewport notify
- [x] 3.4 Bind field-focus, mode toggles, Esc discard, scrub/commit → existing `inspectorTransformEdited` path
- [x] 3.5 Extend `NativeFloatPanelSnapshot` / floating host push for new inspector props

## 4. Verification

- [x] 4.1 Re-run `inspector_transform_ops_test`
- [ ] 4.2 Manual smoke: single-select type/scrub/slider, Scale link, Quaternion normalize, multi Absolute/Delta, rotation delta, focus lock + Esc
- [ ] 4.3 Mark OpenSpec tasks complete; archive when ready per project workflow
