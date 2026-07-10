## Context

Blunder's editor viewport uses an off-screen Vulkan pass + Slint/Skia composite (`docs/agents/render-pipeline.md`). Nsight Group A (Resolved) shows **~5.18 `vkQueueSubmit`/s** during typical orbit while GPU work averages **~0.17 ms** — the limit is software pacing, not the 3D renderer.

Current throttles in `slint_system.cpp`:

| Mechanism | Location | Effect |
|-----------|----------|--------|
| Composite **request** floor | `wouldRequestViewportCompositePeek` | `max(adaptive, **250 ms**)` → ~4 Hz cap |
| Skia **present** interval | `endFrame` when `m_viewport_frame_ready` | `max(env, 50 ms)` default for viewport updates |
| Present gate | `shouldPresentSkiaFrame` | Only when `m_viewport_frame_ready` |
| Zero-copy camera skip | `RenderSystem::tickVulkan` | Skips full Vulkan record if camera moved but composite not scheduled |

`RenderSystem` already skips Vulkan when camera/scene/size unchanged; scene edits use `requestViewportRedraw()` → `m_force_viewport_render`.

## Goals / Non-Goals

**Goals:**

- **Interactive tier** (camera orbit/pan/look, scroll zoom, transform gizmo drag): target **20–30 Hz** viewport updates (default **33 ms** ≈ 30 Hz) for composite request and Skia present.
- **Idle tier** (viewport static): target **5–10 Hz** (default **100 ms** ≈ 10 Hz) to limit Skia/CPU cost.
- Tier selection must not break resize defer, dock splitter defer, or partial Skia repaint.
- Measurable via Nsight `vkQueueSubmit` count during 15 s orbit capture.

**Non-Goals:**

- Changing Nsight capture presets or analysis workflow.
- 60 Hz viewport parity with the main loop.
- Runtime render-scale / shadow toggles (existing env vars stay as-is).
- Timeline semaphore / Skia–Vulkan explicit sync (future work).
- Rewriting the Slint fork partial-rendering path.

## Decisions

### 1. Two-tier pacing with interaction detection

**Decision:** Add `ViewportPacingTier { idle, interactive }` computed each frame from editor input state.

**Signals for interactive tier:**

- `EditorCamera::isViewportInteracting()` — new public API: `m_interaction_mode != none` OR non-zero scroll accumulator this frame.
- `TransformGizmoController::isDragging()` — gizmo manipulation.
- Optional **hold-off timer**: remain interactive for **150 ms** after last signal (smooth release frame).

**Alternatives considered:**

- Timer-only (always 30 Hz) — wastes Skia work when idle.
- Nsight-driven adaptive only — no user-visible tier control; harder to validate.

### 2. Replace the 250 ms hard floor with tier-aware intervals

**Decision:** Change `wouldRequestViewportCompositePeek` to use:

```
min_gap_ns = tier == interactive
    ? max(g_adaptive_viewport_present_ns, editorViewportInteractiveRequestMs())
    : max(g_adaptive_viewport_present_ns, editorViewportIdleRequestMs())
```

Defaults: **33 ms** (interactive), **100 ms** (idle). Remove the literal `250'000'000ull` constant.

**Env vars (new / documented):**

| Variable | Default | Meaning |
|----------|---------|---------|
| `BLUNDER_EDITOR_VIEWPORT_PRESENT_MS` | 50 | Skia present min interval (viewport updating); unchanged name, still used in `endFrame` |
| `BLUNDER_EDITOR_VIEWPORT_INTERACTIVE_MS` | 33 | Composite **request** min interval while interactive |
| `BLUNDER_EDITOR_VIEWPORT_IDLE_MS` | 100 | Composite **request** min interval while idle |
| `BLUNDER_EDITOR_VIEWPORT_INTERACTIVE_HOLD_MS` | 150 | Grace after interaction ends before idle tier |

Set any value to `0` to disable that specific floor (existing pattern in `editorViewportPresentMinIntervalNs`).

**Alternatives considered:**

- Single knob only — cannot hit both 30 Hz interactive and 10 Hz idle.

### 3. Align Skia present interval with tier in `endFrame`

**Decision:** When `viewport_updating`, use:

```
min_interval_ns = tier == interactive
    ? max(editorViewportPresentMinIntervalNs(), editorViewportInteractiveRequestMs())
    : max(editorViewportPresentMinIntervalNs(), editorViewportIdleRequestMs())
```

Keeps present cadence consistent with composite requests; interactive present defaults to **max(50, 33) = 50 ms** unless user lowers `BLUNDER_EDITOR_VIEWPORT_PRESENT_MS` to 33.

**Rationale:** Present is more expensive than Vulkan submit; using the same tier avoids requesting composites faster than we present.

### 4. Decouple zero-copy Vulkan submit from composite schedule

**Decision:** Remove or narrow `skip_camera_only_zero_copy` in `RenderSystem::tickVulkan`.

When `camera_changed` and tier is **interactive**, always record and submit Vulkan (subject to existing fence slot availability). When **idle**, retain skip if `!wouldScheduleViewportComposite()` to avoid redundant GPU work.

**Alternatives considered:**

- Always submit on camera change — wastes GPU when composite won't run (idle tier); rejected for idle path.
- Never skip — simplest but increases idle GPU load.

### 5. Interaction API on `EditorCamera`

**Decision:** Add `bool isViewportInteracting() const` on `EditorCamera` (public). `SlintSystem` or a small `ViewportPacing` helper queries `RenderSystem` → `EditorCamera` + gizmo controller via `global_context`.

**Alternative:** Duplicate interaction state in `SlintSystem` — rejected (drift risk).

### 6. Validation targets (Nsight)

| Phase | Metric | Pass criterion |
|-------|--------|----------------|
| Interactive | `vkQueueSubmit` / 15 s orbit | **≥ 18/s** (~270 events); target band 20–30/s |
| Idle | static viewport 15 s | **≤ 12/s**; target band 5–10/s |
| Regression | Window title FPS during orbit | Within **10%** of pre-change |
| Regression | No black/corrupt viewport | Manual + partial composite log |

Use existing Group A preset (`profiling/nsight/common.txt`); no capture changes.

## Risks / Trade-offs

| Risk | Mitigation |
|------|------------|
| Skia composite CPU spikes during interactive tier on 4K windows | Partial repaint stays on; document `BLUNDER_EDITOR_RENDER_SCALE` |
| `isViewportInteracting` false during edge cases (keyboard fly) | Include scroll accumulator; extend to WASD fly if needed in apply |
| Hold timer keeps interactive tier after release | 150 ms default; tunable via env |
| Zero-copy + validation off still required for best path | Document in render-pipeline; out of scope for pacing change |
| Adaptive `g_adaptive_viewport_present_ns` could fight tier floors | `max()` composition keeps tier as minimum, adaptive can only slow further |

## Migration Plan

1. Implement behind tier logic (no feature flag — behavior change is the product goal).
2. Validate with Nsight Group A before/after on same scene.
3. Update `docs/agents/render-pipeline.md` and `profiling/nsight/README.md` expected submit rates.
4. Rollback: revert `slint_system.cpp` + `render_system.cpp` + `editor_camera` API if interactive tier regresses UI FPS.

## Open Questions

- Should keyboard fly (WASD) count as interactive before first apply spike? **Recommendation:** yes, if `InteractionMode` or movement keys active — verify in apply.
- Should `BLUNDER_EDITOR_VIEWPORT_PRESENT_MS` default drop from 50 → 33 when interactive tier ships? **Recommendation:** keep 50 initially; user can set 33 after validation.
