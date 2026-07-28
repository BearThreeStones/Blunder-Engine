# Task 5 Report: Align View to Camera / Align Camera to View

**Branch:** `feat/camera-gizmo`  
**Base:** `7d9479c`  
**Commit:** `feat(editor): Align View to Camera and Align Camera to View`

## Summary

Implemented target resolution (TDD), both align actions, keyboard shortcuts, and a single-undo history command for Align Camera to View.

## Deliverables

### `resolveAlignCameraTarget` (TDD)

- **File:** `engine/src/runtime/function/scene/align_camera_target.h`
- **Tests:** `engine/src/tests/align_camera_target_test.cpp` — 7 cases (single camera, multi-select fail, non-camera fail, main/first fallback, empty scene)
- **Rules:** one selected Camera → that entity; multi-select → fail; no selection → Main → else lowest EntityId; no cameras → fail

### Align View to Camera

- **File:** `engine/src/runtime/function/editor/align_camera_actions.cpp`
- Sets editor camera via `setLookAt(position, position + forward)` and `setVerticalFov` (new on `EditorCamera`)
- Forward from scene camera world matrix: `-world[2]` (glTF -Z look)
- **No document history**

### Align Camera to View

- Writes entity local TRS + `CameraComponent.vertical_fov_degrees` from editor camera pose/FOV
- Parent-aware world→local position/rotation helpers
- **`makeAlignCameraToViewCommand`** — single undo step for transform + FOV (`editor_commands.cpp`)

### Shortcuts (`RenderSystem::onEvent`)

Gated by `editorOverlaysEnabled`; skipped when `isTranslateModalSessionActive()`.

| Action | Desktop | Laptop fallback |
|--------|---------|-----------------|
| Align View to Camera | Numpad 0 | Alt+Shift+0 |
| Align Camera to View | Ctrl+Alt+Numpad 0 | Ctrl+Alt+Shift+0 |

### Player

Shortcuts wired only under `editorOverlaysEnabled` overlay path; Player host ignores.

## Review fix (2026-07-28)

### CMake scope cleanup

Reverted unrelated `1b1baa2` CMake pollution:

- `active_scene_display`, `sdl_player_viewport_sink`, `project_manager_*` C++ sources, `open_dirty_scene_dialog.slint`

Kept Task 5 additions only:

- `align_camera_actions.h/.cpp`

Also retained `BlunderPm` / `project_manager.slint` in CMake — required for link because `slint_system.cpp` references `ProjectManagerWindow` from pre-existing branch work (not camera-gizmo); without it `engine_editor` fails LNK2019.

### View menu (OpenSpec)

Added minimal **View** dropdown in `editor_window.slint` (toolbar popup, two items):

- Align View to Camera
- Align Camera to View

Wired via `UiEventKind::alignViewToCamera` / `alignCameraToView` in `ui_host.cpp` (same `align_camera_actions` as shortcuts). Callbacks bound in `slint_system.cpp`.

## Build / test

```
cmake --preset vs2026-debug
cmake --build build/vs2026-debug --config Debug --target align_camera_target_test engine_editor
align_camera_target_test: all passed (7/7)
engine_editor.exe — build OK
```

`editor_commands_test` has a pre-existing link error (`blunder_native_abi_fill_from_process`); align command round-trip test added to source but not executed in this run.

## Files changed

| File | Change |
|------|--------|
| `align_camera_target.h` | New resolve helper |
| `align_camera_target_test.cpp` | New unit tests |
| `align_camera_actions.h/.cpp` | Align View / Align Camera actions |
| `editor_commands.h/.cpp` | `makeAlignCameraToViewCommand` |
| `editor_camera.h/.cpp` | `setVerticalFov` |
| `render_system.cpp` | Shortcut wiring |
| `editor_window.slint` | View menu popup + callbacks |
| `ui_events.h`, `ui_host.cpp` | Menu → align actions dispatch |
| `slint_system.cpp` | Slint callback binding |
| `runtime/CMakeLists.txt`, `tests/CMakeLists.txt` | Build entries (scope-cleaned) |
| `editor_commands_test.cpp` | Align command undo/redo test (source) |
| `openspec/changes/camera-gizmo/tasks.md` | §5 checkboxes |

## Concerns / follow-ups

- Align View uses animated `setLookAt` (smooth transition); instant snap could be added if UX prefers.
- `BlunderPm` / `project_manager.slint` remains in CMake for link; not camera-gizmo scope — consider a follow-up to either wire PM on its own target or guard PM code in `slint_system`.
- `editor_commands_test` link failure is unrelated to this task; run after native ABI stub is linked for test targets.
