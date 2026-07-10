## 1. Interaction detection

- [x] 1.1 Add `EditorCamera::isViewportInteracting() const` (`interaction_mode != none` or scroll delta this frame)
- [x] 1.2 Add viewport pacing query helper (e.g. on `SlintSystem` or small free function) combining camera interaction, gizmo `isDragging()`, and hold timer (`BLUNDER_EDITOR_VIEWPORT_INTERACTIVE_HOLD_MS`, default 150 ms)
- [x] 1.3 Wire hold timer: refresh on any interactive signal; drop to idle tier after hold elapses

## 2. Tier-aware composite scheduling (`slint_system.cpp`)

- [x] 2.1 Add `editorViewportInteractiveRequestMs()` and `editorViewportIdleRequestMs()` env readers (defaults 33 ms / 100 ms)
- [x] 2.2 Replace `250'000'000ull` floor in `wouldRequestViewportCompositePeek` with tier-aware `min_gap_ns`
- [x] 2.3 Pass active tier into `wouldScheduleViewportComposite()` / peek path used by `RenderSystem`
- [x] 2.4 Update `endFrame` Skia `min_interval_ns` to use tier when `viewport_updating` (align with design decision 3)

## 3. Zero-copy Vulkan submit (`render_system.cpp`)

- [x] 3.1 Narrow `skip_camera_only_zero_copy`: skip only when **idle** tier AND `!wouldScheduleViewportComposite()`
- [x] 3.2 Verify interactive orbit still calls `pollZeroCopyAndPresent` and sets `m_viewport_frame_ready` on schedule

## 4. Documentation

- [x] 4.1 Update `docs/agents/render-pipeline.md` — tiered pacing, new env vars, expected submit rates
- [x] 4.2 Update `profiling/nsight/README.md` — validation targets (≥18 submit/s interactive, ≤12 idle) and note Frame duration is not viewport FPS
- [x] 4.3 Update `.cursor/skills/viewport-perf/SKILL.md` env table with interactive/idle knobs

## 5. Validation

- [x] 5.1 Build `engine_editor` (Debug); manual orbit — viewport visibly smoother than ~5 Hz
- [ ] 5.2 Nsight Group A 15 s orbit capture: `vkQueueSubmit` ≥ 18/s (target 20–30/s)
- [ ] 5.3 Nsight 15 s static viewport: `vkQueueSubmit` ≤ 12/s (target 5–10/s)
- [ ] 5.4 Regression: window title FPS during orbit within 10% of baseline; no viewport corruption on resize/dock drag
- [ ] 5.5 Optional: compare with `BLUNDER_VIEWPORT_ZERO_COPY=1` + `BLUNDER_VK_VALIDATION=0` for fence/readback baseline
