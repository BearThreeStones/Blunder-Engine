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
  viewport displays ~1 frame behind the GPU. The zero-copy path uses a
  per-frame fence wait for read-after-write sync; a shared timeline semaphore
  (and double-buffering the off-screen image to remove the write-after-read
  reliance on the render-pass subpass dependency) is the next optimisation.
- `EditorCamera` still receives input in window coordinates; for delta-based
  motion (drag/orbit) this is fine, but absolute-position interactions should
  later be remapped to the central viewport rect.

## See also

- [slint-fork.md](slint-fork.md)
- [coordinate-system.md](coordinate-system.md)
- [golden-principles.md](../golden-principles.md)
- [design-docs/architecture.md](../design-docs/architecture.md)
