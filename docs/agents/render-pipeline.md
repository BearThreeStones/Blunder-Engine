# Render Data Flow

The editor uses an off-screen render target + Slint composition pipeline. The
engine itself never presents to the window: Slint's `SkiaRenderer` owns the
HWND and is in charge of `Present`. Per frame:

```
SDL3 pumpEvents
   └─► WindowSystem ─► layers ─► SlintSystem.processEvent (input forwarded)

RenderSystem::tick(dt, viewport_w, viewport_h)
   ├─ resize OffscreenRenderTarget if Slint reports a new central rect size
   ├─ assemble ForwardFrameState + opaque draw list (N mesh sources)
   └─► ForwardRenderPath::renderFrame
         ├─ RHI beginRenderPass (color + depth clear) on offscreen RT
         ├─ opaque draw list [0..N) (basic.slang, per-slot descriptors)
         ├─ transparent meshes
         ├─ scene overlays (axes, wireframe solids — depth-aware, main color)
         ├─ RHI endRenderPass → SHADER_READ_ONLY
   ├─ OverlaySystem::draw_overlay_lines (OverlayLinePass MRT: grid lines → line_tx)
   ├─ OverlaySystem::draw_overlay_aa (overlay_aa.slang → main color)
   ├─ SsaOPass::apply (composite AO onto main color)
   ├─ OverlaySystem::draw_screen_overlays (ScreenOverlayPass LOAD: navigate gizmo)
   ├─ RHI transitionToCopySource → copyColorToBuffer (staging)
   ├─ RHI transitionToShaderRead
   ├─ submit (fence, no stall)
   └─ pollAndPresent → tryMapSlot(vkGetFenceStatus) → present previous frame

SlintSystem::update()
   ├─ slint::platform::update_timers_and_animations()
   └─ SkiaRenderer.render()  // GPU composite + Present on HWND
            └─ The central `Image` control samples the SharedPixelBuffer
              created from the engine's readback pixels.
```

## Key integration points

| Concern              | Owner                            |
|----------------------|----------------------------------|
| Window / HWND        | `WindowSystem` (SDL3, no `SDL_WINDOW_VULKAN`) |
| Vulkan device        | `VulkanContext` (headless, no surface/swapchain) |
| 3D scene pass        | `ForwardRenderPath` + `OffscreenRenderTarget` (RHI pass API) |
| Per-frame readback   | `RenderSystem::tick` (async submit/fence after forward path) |
| UI composite + Present | `SlintSystem` + `SkiaRenderer` |
| 3D viewport size     | Slint `viewport-width/height` ► `RenderSystem` |
| 3D pixels into UI    | `SlintSystem::setViewportImage`  |

## Overlay phases

`OverlaySystem` — screen HUD must not run in the forward CLEAR pass; SSAO composite clears and rewrites main color:

| Phase | API | When |
|-------|-----|------|
| Scene | `draw_scene_overlays` | Inside forward render pass |
| Lines | `draw_overlay_lines` | After forward; MRT to `OverlayLineTargets` |
| Line AA | `draw_overlay_aa` | Before SSAO; composites onto main color |
| Screen | `draw_screen_overlays` | After SSAO; LOAD pass (`ScreenOverlayPass`) |

Translate handle drags and `G` grab entry both enter `TranslateModalSession`,
which owns screen-space motion, constraint projection, confirm/cancel, and session
feedback. Handle entry confirms on LMB release; grab entry (`G` with a selection)
starts a free view-plane session and confirms on LMB click. While a handle-started
session is active, `TransformGizmoOverlay` draws drag-start constraint guides,
an active-handle drag ghost, the origin dot, and translate handle visibility
rules; grab sessions reuse free motion and cursor/outline feedback but draw no
guides or handle ghost. Reference axis arrows stay visible at the live pivot in
both paths. The outline resolve uses the session-active `color_id` to select the
light transform-active color, and `WindowSystem` keeps the four-way move cursor
for the session lifetime.

## Notes / known limitations

- Slint is source-built from the **fork** submodule (`blunder/v1.16.1` on
  `BearThreeStones/slint`, based on upstream `v1.16.1`). See
  `engine/3rdparty/slint/BLUNDER_PATCHES.md`. The C++ custom platform now defaults
  to **Vulkan** window composition on Windows (`SkiaRenderer::default_vulkan`),
  sharing the engine's headless Vulkan device when possible. Set
  `BLUNDER_SLINT_RENDERER=d3d12` to fall back to the D3D12 backend.
- Rebuild target `slint_cpp` whenever the Slint submodule commit changes.
- **Zero-copy 3D viewport (shared Vulkan device):** when the Slint Skia renderer
  composites on the engine's `VkDevice` (`SkiaRenderer::new_vulkan_shared`), the
  off-screen color `VkImage` can be sampled directly by Skia via
  `BorrowedVulkanTexture` / `Image::create_from_borrowed_vulkan_texture` — no CPU
  readback, no staging copy. **Default:** CPU readback into `setViewportImage`
  (reliable in the editor). Opt in to zero-copy with
  `BLUNDER_VIEWPORT_ZERO_COPY=1` when `SlintSystem::viewportUsesSharedDevice()`.
- **Validation-layer caveat:** Skia's `make_vulkan()` cannot build a context on
  an externally-created device while the Vulkan validation layer is loaded, so
  the shared-device/zero-copy path is unavailable when validation is on. The
  renderer then falls back to a self-owned device + the CPU readback path below.
  Release builds (validation off) get zero-copy automatically; in debug set
  `BLUNDER_VK_VALIDATION=0` to enable it.
- The CPU readback path uses persistently mapped staging buffers and async
  fence polling; `VulkanSync::k_max_frames_in_flight` staging buffers are
  provisioned for double-buffering. The GPU copy runs on frame N; on frame
  N+1, `tryMapSlot` polls `vkGetFenceStatus` and, if signaled, hands the
  already-mapped pointer directly to Slint (no intermediate memcpy). The
  viewport displays ~1 frame behind the GPU.
- **Zero-copy double-buffering:** `OffscreenRenderTarget` owns
  `k_buffer_count` (= `VulkanSync::k_max_frames_in_flight`, 2) independent
  color + depth + framebuffer sets. Each frame, `RenderSystem` sets
  `setActiveBufferIndex(m_current_frame)` before recording; the GPU writes
  buffer[slot] while `pollZeroCopyAndPresent()` presents
  `getImage(completed_slot)` to Slint after the matching fence signals.
  Slint rebinds automatically when the presented `VkImage` handle changes
  (slot alternation or resize). This removes same-image write-after-read
  pressure that previously relied on the render-pass EXTERNAL subpass
  dependency. Viewport latency is ~1–2 frames. **Not in v1:** timeline
  semaphore / Skia wait sync.
- **Partial Skia composite (viewport-only repaint):** when the Slint fork's
  Vulkan partial-rendering path is enabled (default), orbit/interaction marks
  only the central viewport logical rect dirty via
  `SkiaRenderer::mark_dirty_region`; dock resize, window resize, and viewport
  invalidate call `force_full_refresh()` for a safe full-window composite.
  Disable with `BLUNDER_SLINT_PARTIAL=0`. **Zero-copy:** each present marks only
  the viewport logical rect dirty (not a full-window composite); resize/rebind
  still calls `force_full_refresh()`. Debug dirty
  coverage with `SLINT_SKIA_PARTIAL_RENDERING=log` (or `visualize`).
- **Editor performance toggles (default off in editor):**
  - `BLUNDER_EDITOR_SHADOWS=1` — shadow pass (doubles opaque draws for large scenes).
  - `BLUNDER_EDITOR_OVERLAY_AA=1` — full-scene overlay anti-aliasing pass.
  - `BLUNDER_VIEWPORT_ZERO_COPY=0` — force CPU readback even when shared device works.
  - `BLUNDER_EDITOR_RENDER_SCALE=0.75` — render 3D at reduced resolution (0.25–1.0,
    default 1.0); Slint upscales the viewport image.
  - `BLUNDER_EDITOR_VIEWPORT_PRESENT_MS=50` — minimum ms between Skia composites
    while the 3D viewport is updating (default 50). Each composite can still cost
    ~50–120ms on large windows; increase if FPS is still low.
  - **Tiered viewport pacing** (interactive vs idle): composite **request** and Skia
    present floors follow camera/gizmo input plus a short hold after release.
    - `BLUNDER_EDITOR_VIEWPORT_INTERACTIVE_MS=33` — request/present floor while
      interacting (~30 Hz target).
    - `BLUNDER_EDITOR_VIEWPORT_IDLE_MS=100` — request/present floor when static
      (~10 Hz target).
    - `BLUNDER_EDITOR_VIEWPORT_INTERACTIVE_HOLD_MS=150` — remain interactive tier
      after input stops (smooth release).
    - Signals: `EditorCamera::isViewportInteracting()`, transform gizmo drag.
    - Zero-copy path skips redundant Vulkan submits on camera-only moves only in
      **idle** tier when no composite is scheduled.
    - Nsight validation: orbit 15 s → `vkQueueSubmit` ≥ 18/s; static 15 s → ≤ 12/s.
- `EditorCamera` still receives input in window coordinates; for delta-based
  motion (drag/orbit) this is fine, but absolute-position interactions should
  later be remapped to the central viewport rect.

## See also

- [slint-fork.md](slint-fork.md)
- [coordinate-system.md](coordinate-system.md)
- [golden-principles.md](../golden-principles.md)
- [design-docs/architecture.md](../design-docs/architecture.md)
