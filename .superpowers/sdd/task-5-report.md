# Task 5 Report — RenderSystem preview RT + second pass

**Status:** DONE  
**Date:** 2026-07-28  
**Branch:** feat/camera-preview  
**OpenSpec:** camera-preview tasks 3.2, 3.3, 3.4

## Delivered

- `RenderSystem`: lazy `m_camera_preview_offscreen`, single-slot staging readback, `recordCameraPreviewPass` in same CB after main copy, async present via `tryPresentCameraPreview`
- Resolves selection → `resolveCameraPreviewTarget` / `buildCameraPreviewMatrices`; RT size via `computeCameraPreviewRtSize`; skips when collapsed / no Camera / zero content
- Preview `ForwardFrameState`: shading copied from main, `shadows_enabled=false`, `renderFrameTo(..., draw_overlays=false)`
- Forces `m_force_viewport_render` when preview target active
- `SlintSystem::setCameraPreviewImage` / `clearCameraPreviewImage` + content/collapsed stubs (pixel buffer stored; Slint panel bind deferred to Task 6/7)

## Build evidence

```powershell
cmake --build build/vs2026-debug --config Debug --target engine_runtime engine_editor
```

**Result:** exit 0 — `engine_runtime.lib`, `engine_editor.exe` (pre-existing Slint LNK4006 / PCH warnings only).

## Out of scope

- Slint `camera_preview_panel.slint` chrome (Tasks 6/7)
- Hit-test / pick block (Task 5 in OpenSpec section 5)
