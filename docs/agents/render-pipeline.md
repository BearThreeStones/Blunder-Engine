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
   ├─ OverlaySystem::draw_outline (ID prepass + edge resolve → main color)
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
| Outline | `draw_outline` | After forward; ID prepass (`color_id`/`ob_id` packed in `R16_UINT`) + smooth resolve composite; multi-select + transform-drag color |
| Lines | `draw_overlay_lines` | After outline; MRT to `OverlayLineTargets` |
| Line AA | `draw_overlay_aa` | Before SSAO; Blender-style cross-neighbor line composite when `BLUNDER_EDITOR_OVERLAY_AA=1` |
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

**P3 — mid-session constraints:** while a translate modal session is active,
`X`/`Y`/`Z` (with `Shift` for plane constraints) cycle orientation global →
local → free on re-press of the same key; a different key starts a new global
constraint. Middle-mouse drag picks the nearest projected axis and commits it on
release. Typed digits apply a signed distance along the active constraint;
`Enter` confirms the session; `Escape` clears numeric input first, then cancels.
Live constraint changes update guides, origin dot, and plane-center visibility.

### Selection outline (packed ID)

- Prepass writes `R16_UINT` per pixel: `(color_id << 14) | (ob_id & 0x3FFF)`.
- `color_id`: `0` = transform-drag orange while gizmo is dragging; `1` = object-select `#F57011`.
- `ob_id`: unique per selected Hierarchy root; subtree meshes share the root's `ob_id`.
- Resolve: 8-neighbor smooth edge mask; edges vs background or `color_id` change; no seam between co-selected meshes with the same `color_id`.
- **Outline AA:** `outline_resolve.slang` `edgeCoverage()` applies linear smoothstep on neighbor count with `kOutlineEdgeSmoothMin = 0.25` / `kOutlineEdgeSmoothMax = 2.5`. CPU mirror: `outline_aa.cpp` (`outlineEdgeCoverage`). Default-on; no env toggle.
- Resolve samples `object_id` / depth using fragment `SV_Position` pixel coordinates (same extent as prepass RT).
- Hierarchy: click = replace, Shift+click = add, Ctrl+click = toggle.

### Overlay anti-aliasing (Blender three-layer model)

1. **Polyline intrinsic AA** — `transform_gizmo.slang` polylines + arrow stems: Blender `smoothline` formula (`kPolylineSmoothWidth = 1`, CPU: `polylineStrokeAlphaBlender`). Screen pass, not line MRT.
2. **Line MRT composite AA** — `OverlayLinePass` writes color + `packLineData` line target; `overlay_aa.slang` decodes and runs cross-neighbor `line_coverage` (CPU: `overlay_line_aa.cpp`). Gated by `BLUNDER_EDITOR_OVERLAY_AA=1`.
3. **Outline resolve** — v1: 8-neighbor kernel in `outline_resolve.slang`; optional v3: Blender directional `pack_line_data` detect.

Shared line pack encoding: `overlay_line_pack.slang` / `overlay_common_lib.glsl` (`sin_theta * 0.5 + 0.5`, `dist * 0.4 + 0.5`).

### Viewport mesh picking

Left-click inside the editor viewport selects scene meshes via `ViewportPickSystem` using the **GPU entity-ID prepass only** (`PickOverlay`).

**Input priority (left-click):**

1. `TransformGizmoController` (handle drag)
2. Navigate gizmo
3. Viewport mesh pick (`ViewportPickSystem::onViewportClick`)
4. Editor camera (orbit/pan does not use left-click)

**Gizmo hover:** On `MouseMoved` inside the viewport (when transform gizmo mode is active and not dragging), `TransformGizmoController::updateHoverFromPointer` runs the same CPU analytic pick as click/drag. Colors follow Blender `gizmo_color_get` via `gizmoColorGet`: normal handles use theme RGB at alpha **0.6** × `alpha_fac` (`color`), hovered interactive handles use the same RGB at alpha **1.0** × `alpha_fac` (`color_hi`). Decor axes (`rot_t`, `rot_c`, `scale_c_outer`) never select `color_hi` — `gizmoHandleUsesHoverColorHi` mirrors Blender `WM_GIZMO_DRAW_HOVER`. Theme RGB matches Blender default `userdef_default_theme.c` (`TH_AXIS_*`, white `TH_GIZMO_VIEW_ALIGN`). Redraw occurs only when the hovered axis changes. Hover does **not** set `event.handled` and does not change selection.

**Transform gizmo SDF AA:** Rotate dials, outer ring, scale annulus, ghost arc, and plane borders draw via local 2D SDF quads in `transform_gizmo.slang` (`ScreenOverlayPass`). Translate/scale **arrow stems** use navigate-gizmo-style view-space arm quads (`vsNavigateStyleArmQuad`). SDF paths use `strokeCoord` markers (`kStrokeSdfRing`, `kStrokeSdfDisc`, `kStrokeSdfRect`); solids and arms use sentinel `> 1`. CPU mirrors: `gizmo_sdf_aa.cpp`. Legacy `gizmo_polyline_aa.cpp` remains for Blender polyline stroke math.

**Navigate gizmo style:** `NavigateGizmoOverlay` draws Blender `VIEW3D_GT_navigate_rotate` at 80px diameter (10px margin). Colors use `navigate_gizmo_style` depth fade against `viewBgColor` in `navigate_gizmo.slang`. On `MouseMoved`, `updateHoverFromPointer` sets hover backdrop (`navigateGizmoHighlightBackdropColor`, gray 0.5) and per-axis label highlight — same redraw-on-change pattern as transform hover.

**Input priority (right-click):**

1. Gizmo / navigate gizmo (same as left-click when applicable)
2. **Ctrl+right-click** in viewport → piercing menu (`ViewportPickSystem::onPiercingMenuClick`)
3. Editor camera free-look (plain right-drag)

**GPU pick (hybrid broad + narrow):**

| Operation | Implementation |
|-----------|----------------|
| Instance buffer (P1) | `PickInstanceBuffer` rebuilds on scene dirty (`syncSceneToRender` / `tickVulkan`), not per click; caches `PickDraw[]` + `PickInstanceGpu` SSBO |
| Left-click pick | Release → **sync narrow** front-most (`pickEntityAtWindowPosition` + promote) → immediate `applySelection` in input phase → sync peel list for cycling → async **broad compute** + narrow refines `m_last_peel_hits` |
| Piercing menu (Ctrl+right) | Broad compute only; `peel_hits` carries promoted hit list |
| Async delivery | `pollHybridPick` at **start** of `tickVulkan` (before viewport skip/render) |
| Selection present | `onSelectionChanged` → `markViewportDirtyRegion()` + `requestViewportRedraw` |
| Transform edit (gizmo / Inspector) | Gizmo `markSceneDirty()` → `notifyViewportAfterGizmoTransformEdit` / `requestViewportRedraw`; Inspector `applyInspectorTransform` → `notifyViewportAfterInspectorTransformEdit` (`markViewportDirtyRegion` + `requestViewportRedraw`) (static camera must not skip the offscreen pass) |
| Piercing menu input | Menu visible → `shouldRouteMouseToInputLayers` false; menu row select suppresses matching left-release pick |

**Selection modifiers:**

| Input | Effect |
|-------|--------|
| Left-click | Replace selection |
| Shift+left-click | Add to selection |
| Ctrl+left-click | Toggle selection |
| Ctrl+right-click | Piercing menu → replace from menu |
| Ctrl+Shift+right-click | Piercing menu → add from menu |
| Repeat left-click within 3 px | Cycle through `m_last_peel_hits` (sync peel list on first click; async broad list refines when ready) |

- GPU pick rasterizes the **leaf entity** owning the hit `MeshRendererComponent`, then promotes to the **scene-root child**: walk up while the current entity's parent has a valid grandparent, then select that parent (e.g. `node_prim0` → `BoxFront` in pick_test). Entities with no parent are returned unchanged.
- Blend-transparent meshes are not pickable.
- Click on empty viewport clears selection (same modifier semantics as Hierarchy).
- `requestPick` marks the viewport dirty so peel passes progress and outline/gizmo refresh without camera movement.

### Pick multi-hit (broad phase)

Editor camera uses **`glm::perspectiveZO`**. Pick prepass uses depth clear `0.0` and `VK_COMPARE_OP_GREATER` for front-most narrow hits.

**Multi-hit** (piercing menu, same-pixel cycling) uses **compute broad phase** (`pick_broad_phase.slang` ray vs world AABB per `PickInstanceGpu`), not iterative entity-ID peel:

1. `PickBroadPhase` dispatch → read back leaf hits with distance `t`
2. `sortBroadHitsByDistance` → `promoteAndDedupeBroadHits` → `m_last_peel_hits`
3. Left-click: one narrow raster pass on broad candidate draws only (front-most leaf → promote)

Depth peel shader uniforms (`isPeelPass` / `peelDepth`) remain for legacy sync dev paths only; not used on the hot path.

**Idle viewport:** `pollViewportPickIfActive` runs at the start of every `tickVulkan` (including when the offscreen pass is later skipped for a static camera).

**Sync-then-async left-click:** `onViewportLeftReleased` sync-picks front-most + `applySelection` in the input phase, then **defers** `requestPick(multi_peel)` to the **next** `tickVulkan` so the selection frame can render outline/gizmo and composite before broad-phase GPU work starts. `notifyEditorUi` calls `markFullSkiaRefresh()`; `endFrame` bypasses idle composite pacing when that flag is set. `deliverPickResult` updates `m_last_peel_hits` from the async broad list and skips redundant `applySelection` when the promoted front matches the current primary.

### Pick-test scene (manual QA)

Default editor startup loads **`assets/Scenes/pick_test.scene.asset`** (override with env `BLUNDER_STARTUP_SCENE`, e.g. `assets/Scenes/root.scene.asset`).

| Entity | Position (Z-up) | Role |
|--------|-----------------|------|
| `BoxFront` | `[0, 2.0, 1.0]` | Nearest along +Y |
| `BoxMid` | `[0, 2.9, 1.0]` | Middle (0.9 m spacing, overlapping 1 m cubes) |
| `BoxBack` | `[0, 3.8, 1.0]` | Farthest along +Y |

All three are **root entities** sharing `assets/Meshes/Cube.mesh.yaml`. View the stack along ±Y so the ray through overlapping voxels hits three mesh renderers. glTF import adds `node → node_prim0` under each box; promotion walks up to `BoxFront` / `BoxMid` / `BoxBack`. Hybrid pick QA expects **≥3** piercing-menu rows and **≥3** same-pixel cycle steps with those promoted names. Sync peel list on first click enables same-pixel cycling on the second click; async broad delivery refines the list when ready.

Restore Sponza demo: set `BLUNDER_STARTUP_SCENE=assets/Scenes/root.scene.asset` and spawn `assets/Sponza.mesh.yaml` from the Content Browser.

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
