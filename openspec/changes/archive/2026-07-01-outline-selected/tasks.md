## 1. GPU resources and shaders

- [x] 1.1 Add `OutlineTargets` (`outline_targets.h/.cpp`) with `R16_UINT` object-ID image, `D32` outline-depth image, render pass, and framebuffer; probe format support with `R32_UINT` fallback
- [x] 1.2 Add `outline_prepass.slang` — minimal VS (model/view/proj) + FS writing `uint` ID `1` with depth test/write
- [x] 1.3 Add `outline_resolve.slang` — fullscreen quad, 4-neighbor ID edge detect, hardcoded Blender orange (`#F57011`), scene-depth occlusion (`alpha_occlu = 0.35`, epsilon `3/8388608`)
- [x] 1.4 Wire Slang compilation and Vulkan graphics/compute pipelines for prepass and resolve (follow `OverlayAntiAliasing` / `OverlayLinePass` patterns)

## 2. OutlineOverlay module

- [x] 2.1 Add `OutlineOverlay` (`outline_overlay.h/.cpp`) implementing `Overlay` lifecycle: `begin_sync`, prepass `draw`, resolve `draw_resolve`
- [x] 2.2 In `begin_sync`, resolve selected `EntityId` → `MeshRendererComponent` + `GpuMesh` + world matrix via `SceneInstance`; set `enabled_` when mesh is valid
- [x] 2.3 Implement prepass draw: bind outline pipeline, draw selected mesh index buffer (opaque and blend renderers)
- [x] 2.4 Implement resolve draw: barrier ID/depth targets + scene depth to shader-read, LOAD-pass composite onto main offscreen color

## 3. OverlaySystem and RenderSystem integration

- [x] 3.1 Extend `OverlaySystem` with `OutlineOverlay` + `OutlineTargets`; initialize/resize/shutdown alongside existing overlay resources
- [x] 3.2 Extend `OverlayState` if needed (e.g. cached selection entity id); call `m_outline.begin_sync` from `OverlaySystem::begin_sync`
- [x] 3.3 Insert `draw_outline` calls in `RenderSystem::tickVulkan` after `ForwardRenderPath::renderFrame`, before `draw_overlay_lines`
- [x] 3.4 Add `hasActiveOutline()` or equivalent so outline resolve runs only when enabled

## 5. Bug fixes (post-implementation)

- [x] 5.1 Fix outline prepass vertex layout: use `Vertex::getBindingDescription()` (48-byte stride with tangent) instead of incorrect 32-byte stride
- [x] 5.2 Outline selected entity subtree: when parent has no `MeshRenderer`, draw all descendant mesh renderers in prepass

## 4. Validation and docs

- [x] 4.1 Build target `engine_runtime` (or project validate script)
- [ ] 4.2 Manual: select opaque mesh → orange silhouette outline visible
- [ ] 4.3 Manual: select blend/transparent mesh → outline visible on surface hull
- [ ] 4.4 Manual: clear selection → outline disappears; occluded parts of outline are semi-transparent
- [x] 4.5 Update `docs/agents/render-pipeline.md` overlay phase table with outline prepass + resolve
