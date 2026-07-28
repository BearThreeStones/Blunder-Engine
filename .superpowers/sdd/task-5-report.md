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

---

## Task 5 review fixes (2026-07-28)

**Commit:** `fix(preview): Task 5 review — transparent sort, resize timing, clear paths`

### Fixed

1. **Critical — transparent sort:** `recordCameraPreviewPass` re-sorts `m_transparent_mesh_draws` by distance to preview camera (`cam.position`) and builds a local `preview_transparent_draws` list before `renderFrameTo`.
2. **Important — resize timing:** `ensureCameraPreviewOffscreenIfNeeded()` probes RT size and resizes before command-buffer recording (called in `tickVulkan` alongside main offscreen resize). `recordCameraPreviewPass` no longer calls `ensureCameraPreviewOffscreen` mid-CB.
3. **Important — clear on skip paths:** `clearCameraPreviewPresentation()` + `syncCameraPreviewSkipClear()` clear Slint image and cancel pending readback on collapsed, null scene, missing selection, `!rt_size.ok`, `!cam.ok`, and missing offscreen/staging.
4. **Important — collapse stale present:** `tryPresentCameraPreview` calls `clearCameraPreviewPresentation()` when collapsed; pending readback is dropped.
5. **Optional — map fail:** `resizeCameraPreviewReadback` resets `m_camera_preview_readback_pending` when staging `vmaMapMemory` fails.

### Build evidence

```powershell
cmake --build build/vs2026-debug --config Debug --target engine_runtime engine_editor
```

**Result:** exit 0 — `engine_runtime.lib`, `engine_editor.exe` (pre-existing Slint LNK4006 / PCH warnings only).

### Remaining concerns

- Probe/skip logic is duplicated across `ensureCameraPreviewOffscreenIfNeeded`, `syncCameraPreviewSkipClear`, and `recordCameraPreviewPass` — could be consolidated later.
- First frame after RT size change may skip preview render until next frame (by design; resize is idle-synced before CB).
