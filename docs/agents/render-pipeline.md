# Render Data Flow

The editor uses an off-screen render target + Slint composition pipeline. The
engine itself never presents to the window: Slint's `SkiaRenderer` owns the
HWND and is in charge of `Present`. Per frame:

```
SDL3 pumpEvents
   └─► WindowSystem ─► layers ─► SlintSystem.processEvent (input forwarded)

RenderSystem::tick(dt, viewport_w, viewport_h)
   ├─ resize OffscreenRenderTarget if Slint reports a new central rect size
   ├─ assemble ForwardFrameState + CPU opaque draw list (skinned / no-Meshlet meshes)
   │     + GPU instance buffer (static opaque/alpha-clip MeshRenderers with Meshlets)
   ├─► Frame graph execute (shading + overlays + Copy Sink; Vulkan recorder)
   │     ([ADR 0067](../adr/0067-frame-graph-viewport-wire.md), [ADR 0068](../adr/0068-frame-graph-viewport-overlays.md), [ADR 0069](../adr/0069-frame-graph-deferred-split.md))
   │     ├─ Scene callback ForwardRenderPath::renderFrame            (editor Viewport when BLUNDER_EDITOR_DEFERRED=0; Placement Preview)
   │     │     ├─ shadow pass: mesh-shader VSM pages + point cubes + spot 2D
   │     │     │     (VS/FS classic 1024² directional when mesh shaders are absent;
   │     │     │      GPU-driven static casters are opaque meshlets only; no Hi-Z)
   │     │     ├─ GPU-driven cull (graphics-queue compute): frustum + cone + previous-frame Hi-Z
   │     │     │     → early indirect command list; late list retests last frame's Hi-Z rejects
   │     │     ├─ RHI beginRenderPass (color + depth clear) with SECONDARY contents
   │     │     ├─ execute opaque secondary: GPU-driven early + late (indirect or task/mesh),
   │     │     │     then CPU `vkCmdDrawIndexed` list; scene-overlay secondary; transparent secondary
   │     │     └─ RHI endRenderPass → SHADER_READ_ONLY; depth feeds next frame's Hi-Z pyramid
   │     ├─ or Deferred: viewport.gbuffer then viewport.lighting (editor default; Player always)
   │     │     ├─ G-buffer callback: shadow fill (VSM/locals or classic 1024), then G-buffer RP
   │     │     │     After G-buffer: VSM page mark from offscreen depth (not froxel lists)
   │     │     │     (GPU-driven static early/late + CPU skinned, alpha clip, Bindless set 1; no offscreen color)
   │     │     └─ Lighting callback: offscreen color CLEAR, `deferred_lighting` secondary
   │     │           (fullscreen triangle; Deferred light list 32 + mask SSBO 16384 slots, ≤ 8 lights/slot;
   │     │            Directional VSM PCF + Point cube / Spot 2D PCF; no Bindless)
   │     │           optional image-based VRS (`VK_KHR_fragment_shading_rate` attachment, previous-frame Sobel 1×1/2×2)
   │     │           then Sobel compute writes this FIF's rate image; editor Viewport may blit the rate-mask overlay
   │     │           then PRIMARY barriers → OffscreenRenderTarget LOAD color + depth
   │     │           execute scene-overlay secondary, then transparent secondary (Forward PBR)
   │     │           endLoadRenderPass → SHADER_READ_ONLY / DEPTH_STENCIL_READ_ONLY
   │     ├─ Outline (optional; `hasActiveOutline`)
   │     ├─ Line+AA (optional; `hasActiveLineOverlays`; one Pass)
   │     ├─ SSAO (optional; `ssao_enabled`)
   │     ├─ Screen overlays (when OverlaySystem exists)
   │     └─ Copy Sink (zero-copy shader-read vs CPU copy then shader-read)
   ├─ Camera Preview (dedicated DeferredRenderPath on the 480px RT; after this execute)
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
| 3D scene pass        | Frame graph shading Passes (`execute` + Vulkan recorder): Forward/Player Scene ([ADR 0067](../adr/0067-frame-graph-viewport-wire.md)); editor Deferred G-buffer then Lighting ([ADR 0069](../adr/0069-frame-graph-deferred-split.md), path [ADR 0062](../adr/0062-deferred-render-path.md)). Overlay/copy Passes on that graph: [ADR 0068](../adr/0068-frame-graph-viewport-overlays.md) |
| Static geometry submission | GPU-driven rendering inside the Forward / G-buffer / shadow passes ([ADR 0070](../adr/0070-gpu-driven-rendering.md)); CPU draw list for the remainder (see below) |
| Per-frame readback   | `RenderSystem::tick` (async submit/fence after the viewport Render Path) |
| UI composite + Present | `SlintSystem` + `SkiaRenderer` |
| 3D viewport size     | Slint `viewport-width/height` ► `RenderSystem` |
| 3D pixels into UI    | `SlintSystem::setViewportImage`  |

## GPU-driven rendering (static opaque / alpha clip)

Static opaque and alpha-clip `MeshRendererComponent`s whose cooked mesh Final
contains Meshlets submit through the GPU instead of the CPU draw list
([ADR 0070](../adr/0070-gpu-driven-rendering.md); terms: [CONTEXT.md — GPU-driven
rendering, Meshlet, Hi-Z](../../CONTEXT.md)). Shading still runs on the Forward or
Deferred Render Path; this is a geometry-submission change, not a visibility buffer
and not Bindless.

| Piece | Detail |
|-------|--------|
| Instance buffer | Per-frame GPU buffer of static opaque/alpha-clip MeshRenderers with Meshlets: world matrix, Bindless texture indices, Meshlet range, MeshRenderer receiver id. Meshes cooked without Meshlets (version ≤ 2 Finals, skinned) stay on the CPU `ForwardOpaqueDraw` list. |
| Cull | Graphics-queue compute (no dedicated compute queue): frustum + Meshlet cone cull, then previous-frame Hi-Z. Writes indirect draw commands / Meshlet lists. Exact-match FATAL on the compute layout. |
| Hi-Z | Path-owned depth pyramid built from the **previous** frame's offscreen depth, one per `OffscreenRenderTarget::k_buffer_count` slot (FIF like the G-buffer). Not a Frame graph Transient. First frame: empty pyramid, frustum-visible Meshlets draw. **Not** a depth prepass and **not** a visibility buffer. |
| Early / late | Early pass draws Meshlets that pass frustum + cone + previous-frame Hi-Z. Late pass draws the Hi-Z rejects that still pass frustum + cone, in the **same** opaque secondary (no split of the CLEAR pass). The pyramid is built **after** the pass for the **next** frame. First frame: `hiz_enabled=0`. |
| Draw | `vkCmdDrawIndexedIndirect` (count variant when available) with the existing PBR / G-buffer fragment shaders + Bindless set 1. With `VK_EXT_mesh_shader`: task + mesh shaders consume the early/late Meshlet lists and emit the same fragment inputs as the VS path. |
| Mesh shader fallback | `VK_EXT_mesh_shader` is queried at device create; a missing extension **must not** fail device init. Without it the path is compute cull + traditional VS/FS indirect draws. Exact-match FATAL on task/mesh layouts when present. |
| Directional shadows | Mesh-shader clipmap (128² pages, 512 physical D32 pages, FirstLevel 6…LastLevel 10) filled from opaque meshlets. Fallback device: classic 1024² ortho VS fill. Skinned / alpha / foliage do not cast. |
| Point / Spot shadows | Up to 8 cubemaps and 8 perspective 2D maps per view, same opaque-meshlet fill, 2×2 PCF. Not VSM. |
| Deferred | GPU-driven static writes the G-buffer (alpha clip in geometry). Receiver plane is `R16_UINT` with a 14-bit MeshRenderer slot + unlit + two-sided bits; Meshlets inherit their MeshRenderer id (no per-Meshlet id). `deferred_lighting.slang` decodes that packing. |
| CPU list remainder | Skinned, blend-transparent, pick, outline stay on the CPU draw list with the Forward mesh draw cap (256 per-draw constant slots). GPU-driven static does not use that cap. Camera Preview / Mesh Preview / Thumbnail are deferred lighting but still gather CPU draws (no GPU-driven on those surfaces). |
| Cook | Meshlets (64 verts / 124 tris, sphere + cone via MeshOptimizer) are appended to the mesh Final at Cook (`kMeshCookVersion` = 3). `.meshbin.meta` records `cook_format` (missing key reads as 0); `AssetCompilerService` treats a mesh Final as stale when that value differs from `kMeshCookVersion` and recooks it. Texture metas do not require `cook_format`. |
| Demo scene | Sponza lives in the **Test project only**: `Assets/Scenes/sponza.scene.asset` (Main Camera, Directional Light, existing `Sponza.mesh.yaml` through the glTF importer). No Crytek files are copied into the engine repo and no engine test cooks Sponza. GPU-driven is the default path, not a per-scene switch. |

## Overlay phases

`OverlaySystem` — screen HUD must not run in the forward CLEAR pass; SSAO composite clears and rewrites main color:

| Phase | API | When |
|-------|-----|------|
| Scene | `draw_scene_overlays` | Forward: inside the forward CLEAR pass between opaque and transparent. Deferred: inside the offscreen LOAD pass after lighting, before transparent (never into the G-buffer) |
| Outline | `draw_outline` | Frame graph Pass after Scene (Forward) or after Lighting (Deferred); ID prepass (`color_id`/`ob_id` packed in `R16_UINT`) + smooth resolve composite; multi-select + transform-drag color |
| Lines | `draw_overlay_lines` | Frame graph Line+AA Pass after outline; MRT to `OverlayLineTargets` |
| Line AA | `draw_overlay_aa` | Same Line+AA Pass, before SSAO; Blender-style cross-neighbor line composite when `BLUNDER_EDITOR_OVERLAY_AA=1` |
| Screen | `draw_screen_overlays` | Frame graph Pass after SSAO; LOAD pass (`ScreenOverlayPass`); Copy is the Sink |

### Deferred Render Path

`DeferredRenderPath` (`function/render/deferred/`) is a hardcoded sibling of
`ForwardRenderPath` for G-buffer vs lighting ([ADR 0062](../adr/0062-deferred-render-path.md)).
Editor and Player Deferred dispatch is a G-buffer Pass then a Lighting Pass
([ADR 0069](../adr/0069-frame-graph-deferred-split.md)); Placement Preview stays
one Forward Scene Pass ([ADR 0067](../adr/0067-frame-graph-viewport-wire.md)). Overlay,
SSAO, and copy are later Passes on that same graph; Copy is the Sink
([ADR 0068](../adr/0068-frame-graph-viewport-overlays.md)). `RenderSystem` creates
the Viewport/Player Deferred path unless the host is the editor **and**
`BLUNDER_EDITOR_DEFERRED=0`. Camera Preview, Mesh Preview, and Scene Thumbnail /
Capture each own a Deferred path on their Offscreen (VRS attaches to that lighting
RP). Image-based VRS: [ADR 0074](../adr/0074-image-based-variable-rate-shading.md).

| Piece | Detail |
|-------|--------|
| G-buffer | Path-owned extra images, one set per `OffscreenRenderTarget::k_buffer_count` slot: `RGBA8` albedo+AO, `RGBA8` oct-normal.xy+metallic+roughness, `R16_UINT` receiver (14-bit MeshRenderer slot bits 0–13, unlit bit 14, two-sided bit 15; clear `0xFFFF` = no geometry). Meshlets inherit their MeshRenderer id. Depth attachment is the offscreen depth of that slot. Not an `OffscreenRenderTarget` MRT. |
| Geometry shaders | `gbuffer.slang` / `gbuffer_skinned.slang` (Bindless set 1 like Forward opaque; same Matrix Palette UBO as `pbr_skinned.slang`; alpha clip in geometry). GPU-driven static uses the same G-buffer fragment through indirect / mesh-shader geometry; skinned stays `vkCmdDrawIndexed`. Pipelines use `GraphicsPipelineDesc::color_attachment_count = 3`. |
| Lighting | `deferred_lighting.slang` fullscreen triangle into the offscreen color (CLEAR to `kViewportBackgroundRgb`). UBO = Deferred light list (`buildDeferredFullscreenLightList`, cap 32) + shadow sampling uniforms. Per-slot 32-bit masks from `evaluateLightsForReceiver` (Light linking + cap 8) live in an SSBO of 16384 uints (CPU slots 0–255, GPU-driven 256–16383). Reconstructs world position from depth; samples directional VSM (or classic 1024) plus Point cubemaps and Spot 2D maps with 2×2 `SampleCmp` PCF. Clustered point/spot sample those maps. No Bindless. |
| After lighting | Sobel compute (when VRS is on) writes this FIF's `R8_UINT` rate image. Editor Viewport may blit the two-colour rate-mask overlay. Then PRIMARY barriers, then `OffscreenRenderTarget::beginLoadRenderPass` (LOAD color + depth) executing `forward_scene_overlay` then `forward_transparent` secondaries via `ForwardRenderPath::recordSceneOverlayAndTransparent`. Outline / lines / AA / SSAO / screen overlays / copy are Frame graph Passes after this Lighting Pass ([ADR 0068](../adr/0068-frame-graph-viewport-overlays.md)). |
| VRS | Optional `VK_KHR_fragment_shading_rate` lighting attachment ([ADR 0074](../adr/0074-image-based-variable-rate-shading.md)). Previous-frame Sobel Rec.709 `G > 0.1` → 1×1 else 2×2. `BLUNDER_EDITOR_VRS=0` force-off. No software VRS. Missing extension logs, no FATAL. |
| Secondaries | New `SecondaryPass::gbuffer_opaque` and `SecondaryPass::deferred_lighting`; layout barriers stay on the PRIMARY (ADR 0059). |
| Layout | Exact-match FATAL via `fillGBufferExpectedBindings` / `fillDeferredLightingExpectedBindings` / `fillVrsSobelExpectedBindings` / `fillVrsRateMaskExpectedBindings`; covered by `shader_resource_layout_test`. CPU twin: `vrs_rate_test`. |

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
| Animation preview / cine play | `notifyViewportAfterAnimationPreviewFrame` each tick while playing (`markViewportDirtyRegion` + `requestViewportRedraw`) so a static camera still presents skinned poses; `syncAnimationWindowPlaybackClock` pushes playhead / clip length to the Animation window |
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

Sponza demo (Test project): set `BLUNDER_STARTUP_SCENE=assets/Scenes/sponza.scene.asset`, or open it from the Content Browser. The scene references the existing `assets/Sponza.mesh.yaml` glTF import; nothing Crytek lives in the engine repo.

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
  timeline polls; `VulkanSync::k_max_frames_in_flight` staging buffers are
  provisioned for double-buffering. The GPU copy runs on frame N; on frame
  N+1, `tryMapSlot` polls the slot timeline value (`vkWaitSemaphores` with
  timeout 0) and, if reached, hands the already-mapped pointer directly to
  Slint (no intermediate memcpy). The viewport displays ~1 frame behind
  the GPU. See [ADR 0060](../adr/0060-vulkan-timeline-semaphores.md).
- **Zero-copy double-buffering:** `OffscreenRenderTarget` owns
  `k_buffer_count` (= `VulkanSync::k_max_frames_in_flight`, 2) independent
  color + depth + framebuffer sets. Each frame, `RenderSystem` sets
  `setActiveBufferIndex(m_current_frame)` before recording; the GPU writes
  buffer[slot] while `pollZeroCopyAndPresent()` presents
  `getImage(completed_slot)` to Slint after the matching timeline value is
  reached. Slint rebinds automatically when the presented `VkImage` handle
  changes (slot alternation or resize). This removes same-image
  write-after-read pressure that previously relied on the render-pass
  EXTERNAL subpass dependency. Viewport latency is ~1–2 frames. **Not in
  v1:** Skia wait sync on the timeline (Slint still presents about one
  frame behind).
- **Partial Skia composite (viewport-only repaint):** when the Slint fork's
  Vulkan partial-rendering path is enabled (default), orbit/interaction marks
  only the central viewport logical rect dirty via
  `SkiaRenderer::mark_dirty_region`; dock resize, window resize, and viewport
  invalidate call `force_full_refresh()` for a safe full-window composite.
  Disable with `BLUNDER_SLINT_PARTIAL=0`. **Zero-copy:** each present marks only
  the viewport logical rect dirty (not a full-window composite); resize/rebind
  still calls `force_full_refresh()`. Debug dirty
  coverage with `SLINT_SKIA_PARTIAL_RENDERING=log` (or `visualize`).
- **Editor performance toggles:**
  - `BLUNDER_EDITOR_SHADOWS=0` — debug *off* switch for Viewport shadows (default is on when casters exist). Player ignores this.
  - `BLUNDER_EDITOR_DEFERRED=0` — force the editor viewport onto the Forward Render Path. Default Deferred. Player always Deferred. Camera Preview / Mesh Preview / Thumbnail are deferred on their own offscreen. Placement Preview stays Forward. See [ADR 0062](../adr/0062-deferred-render-path.md).
  - `BLUNDER_EDITOR_VRS=0` — force-off image-based VRS on deferred lighting even when `VK_KHR_fragment_shading_rate` exists. Missing extension already skips VRS (no FATAL). See [ADR 0074](../adr/0074-image-based-variable-rate-shading.md).
  - `BLUNDER_EDITOR_OVERLAY_AA=1` — full-scene overlay anti-aliasing pass.
  - `BLUNDER_VIEWPORT_ZERO_COPY=0` — force CPU readback even when shared device works.
  - `BLUNDER_EDITOR_RENDER_SCALE=0.75` — render 3D at reduced resolution (0.25–1.0,
    default 1.0); Slint upscales the viewport image.
  - `BLUNDER_EDITOR_VIEWPORT_PRESENT_MS=50` — Skia floor while the 3D viewport is
    updating **and idle** (default 50). Interactive orbit/gizmo uses
    `BLUNDER_EDITOR_VIEWPORT_INTERACTIVE_MS` only (not stacked on this 50ms),
    so camera motion is not capped at 20 Hz. Each composite can still cost
    ~50–120ms on large windows; increase the idle floor if background FPS is still low.
  - **Tiered viewport pacing** (interactive vs idle): composite **request** and Skia
    present floors follow camera/gizmo input plus a short hold after release.
    - `BLUNDER_EDITOR_VIEWPORT_INTERACTIVE_MS=33` — request/present floor while
      interacting (~30 Hz target).
    - `BLUNDER_EDITOR_VIEWPORT_IDLE_MS=100` — request/present floor when static
      (~10 Hz target).
    - `BLUNDER_EDITOR_VIEWPORT_INTERACTIVE_HOLD_MS=150` — remain interactive tier
      after input stops (smooth release).
    - Signals: `EditorCamera::isViewportInteracting()`, transform gizmo drag,
      and camera pointer capture (`WindowSystem::getFocusMode()`).
    - RMB/MMB orbit **does not** enable SDL relative mouse, mouse grab, cursor
      hide, or auto-capture. Orbit applies `MouseMovedEvent` deltas. Layout
      cooldown cannot skip `rendererTick` while the camera is interacting.
      Slint does not receive pointer events during orbit. After each Skia
      present the HWND compositor is flushed (`DwmFlush`). The Skia swapchain
      prefers `IMMEDIATE` so DWM cannot queue FIFO frames until mouse-up.
      Do **not** `waitSlot` on the UI thread during orbit (that blocked pumping
      for ~1s). Do **not** set `WS_EX_NOREDIRECTIONBITMAP` on this HWND (Skia
      Vulkan present still needs DWM's redirection bitmap; the style made the
      editor window fully transparent). The SDL event drain is bounded so
      `tickOneFrame` still presents while the button is held.
    - Zero-copy path skips redundant Vulkan submits on camera-only moves only in
      **idle** tier when no composite is scheduled. A completed GPU image always
      sets `viewport_frame_ready`; Skia present interval is applied only in
      `endFrame`.
    - Nsight validation: orbit 15 s → `vkQueueSubmit` ≥ 18/s; static 15 s → ≤ 12/s.
- `EditorCamera` still receives input in window coordinates; for delta-based
  motion (drag/orbit) this is fine, but absolute-position interactions should
  later be remapped to the central viewport rect.

## See also

- [slint-fork.md](slint-fork.md)
- [coordinate-system.md](coordinate-system.md)
- [golden-principles.md](../golden-principles.md)
- [design-docs/architecture.md](../design-docs/architecture.md)
