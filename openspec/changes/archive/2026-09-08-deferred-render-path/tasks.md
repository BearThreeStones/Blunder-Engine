## 1. Deferred light list CPU

- [x] 1.1 Add `buildDeferredLightList` (Active in Hierarchy, Light enabled, not a contribution no-op, EntityId order, cap 32) and `evaluateLightsForReceiver` (Light linking + Light evaluation cap 8) next to `gatherLightsForMesh`. Per-receiver result for a mesh whose lights all fit in 32 SHALL match `gatherLightsForMesh`.
- [x] 1.2 Extend `light_eval_test`: linking exclude; ninth of nine empty-linking points dropped per receiver; 33rd light absent from the Deferred light list; 32-list then per-receiver 8 matches `gatherLightsForMesh` for a sampled mesh.
- [x] 1.3 Build and run `light_eval_test` (`build/vs2026-debug`, Debug).

## 2. G-buffer and shaders

- [x] 2.1 Add G-buffer images (albedo+AO, oct-normal+metallic+roughness, R16 receiver) sized with the viewport, double-buffered with offscreen slots, sharing offscreen depth. Clear receiver to a no-geometry sentinel. Do not expand OffscreenRenderTarget into a permanent MRT.
- [x] 2.2 Engine shaders: G-buffer geometry (static + skinned, Bindless color textures, alpha clip, Matrix Palette on skinned) and fullscreen lighting (G-buffer + depth reconstruct, Deferred light list UBO, shadow comparison binding, unlit albedo out, empty → viewport background). Exact-match FATAL on each new pipeline’s Shader resource layout.
- [x] 2.3 Lighting fills the Deferred light list + per-draw slot → MeshRenderer id table. Sample the existing Directional shadow map with the same PCF as `pbr.slang` when that light’s shadow flag is set.

## 3. DeferredRenderPath and viewport

- [x] 3.1 Add `DeferredRenderPath` and `BLUNDER_EDITOR_DEFERRED` (truthy) on the editor viewport only. Unset → existing `ForwardRenderPath::renderFrame`. Player / Camera Preview / Mesh Preview / Thumbnail ignore the env.
- [x] 3.2 Pass order: existing shadow pass; G-buffer SECONDARY; lighting into offscreen color (CLEAR); LOAD offscreen color+depth for scene overlay then transparent Forward secondaries; then outline / lines / AA / SSAO / screen overlays unchanged. New `SecondaryPass` slots for G-buffer and lighting. Barriers on PRIMARY.
- [x] 3.3 Opaque static + skinned write G-buffer (draw cap still 256). Transparent (including skinned) stay Forward after-pass with today’s gather. Scene overlays are not recorded into the G-buffer pass.

## 4. Layout test and docs

- [x] 4.1 Update `shader_resource_layout_test` expected bindings for the new Engine shaders. Existing `pbr.slang` / skinned extracts still pass.
- [x] 4.2 Build and run `shader_resource_layout_test` (`build/vs2026-debug`, Debug).
- [x] 4.3 Keep [ADR 0062](../../../docs/adr/0062-deferred-render-path.md) and CONTEXT Deferred terms aligned. Update `docs/agents/render-pipeline.md` tick diagram, overlay table, and `BLUNDER_EDITOR_DEFERRED`. Extend CONTEXT **Secondary command buffer** pass list. Do not put this path on Frame graph GPU.
