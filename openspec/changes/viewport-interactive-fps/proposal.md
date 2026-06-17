## Why

The editor 3D viewport updates at roughly **4–5 Hz** during camera orbit/pan while the main loop stays smooth (~60 FPS). Nsight profiling shows GPU 3D work is cheap (~0.17 ms); the bottleneck is **application-side throttling** in `SlintSystem` (250 ms composite-request floor, 50 ms Skia present interval) and **zero-copy camera skip** in `RenderSystem`. Users expect a responsive viewport during interaction without paying full-window Skia composite cost every frame when idle.

## What Changes

- Introduce **two-tier viewport frame pacing**: **interactive** (~20–30 Hz) while the user is actively manipulating the viewport camera or gizmo, **idle** (~5–10 Hz) when the viewport is static.
- Replace the hard-coded **250 ms** minimum between viewport composite *requests* with tier-aware intervals driven by env-tunable defaults.
- **Decouple Vulkan submit from Skia composite scheduling** on the zero-copy path so camera motion still records GPU frames even when a Skia present is not yet due (present remains throttled).
- Preserve existing defer paths (window resize, dock splitter, `shouldDeferHeavyFrameWork`) and partial Skia repaint behavior.
- Add Nsight-friendly validation targets (`vkQueueSubmit` rate during orbit vs idle) documented in tasks; no change to Nsight capture presets.

## Capabilities

### New Capabilities

- `editor-viewport-pacing`: Tiered viewport update rates (interactive vs idle) for Vulkan submit, composite request, and Skia present; interaction detection and env overrides.

### Modified Capabilities

- _(none — no existing OpenSpec specs in repo)_

## Impact

- **Primary**: `engine/src/runtime/function/slint/slint_system.cpp` (composite request/present throttles, interaction state).
- **Secondary**: `engine/src/runtime/function/render/render_system.cpp` (zero-copy camera-only skip), `engine/src/runtime/function/render/editor_camera.cpp` or input path (interaction signals).
- **Docs**: `docs/agents/render-pipeline.md`, `profiling/nsight/README.md` (expected submit rates).
- **Env vars**: extend/document `BLUNDER_EDITOR_VIEWPORT_PRESENT_MS` and add interactive-tier knobs (see design).
- **Risk**: Higher CPU/Skia load during drag on large windows; mitigated by partial composite and idle tier.
