# Task 4 Report — Camera Gizmo FOV/clip handles + Document History

**Status:** DONE  
**Date:** 2026-07-28  
**Branch:** feat/camera-gizmo

## Built

| Area | Location |
|------|----------|
| FOV/clip drag controller | `camera_gizmo_controller.cpp/.h` — press/move/release; live `setCamera`; history on release |
| FOV/clip math helpers | `camera_gizmo_math.h` — clamp FOV 1°–179°, near/far epsilon |
| Handle hit-test | `camera_gizmo_hit_test.h` — `hitTestCameraGizmoHandlesViewportLocal` (FOV top edge, near/far clip frames) |
| Handle draw | `camera_gizmo_overlay.cpp` — yellow handles only for sole selected Camera |
| Document History | `makeSetCameraComponentCommand` in `editor_commands.h/.cpp` — undo/redo via `setCamera`, clears other main |
| Inspector | `SlintSystem::applyInspectorCamera` — same command with before/after snapshot |
| Event wiring | `render_system.cpp` — camera gizmo controller after transform gizmo |

## Behavior

- Handles visible only when exactly one selected entity has `CameraComponent`; multi-select shows gizmo body only (no handles).
- FOV drag: ray-plane intersect on display frame; `verticalFovDegreesFromHalfHeight` with clamp.
- Clip drag: ray-line along look axis; `setCameraNearClip` / `setCameraFarClip` keep near < far.
- Release seals one `makeSetCameraComponentCommand` (Transform gizmo pattern); `suppressNextLeftReleasePick` on press.

## Tests

| Target | Result |
|--------|--------|
| `engine_editor` (Debug) | PASS (build) |
| `camera_gizmo_math_test` | PASS — FOV round-trip, clip plane helpers |
| `editor_commands_test` | Camera undo/redo block added; link fails on `blunder_native_abi_fill_from_process` (pre-existing `engine_runtime` test link issue) |

## Commit

`feat(overlay): Camera Gizmo FOV and clip handles with history`

## Concerns

- `editor_commands_test` does not link in current tree (unrelated native ABI symbol); camera command test is present and compiles against `engine_runtime`.
- Manual QA: drag FOV/clip handles, undo/redo, Inspector FOV commit — deferred to Task 6 checklist.
