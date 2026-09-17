## Context

See proposal.md for why. Domain: [CONTEXT.md — Deferred Render Path](../../../CONTEXT.md). Decision: [ADR 0062](../../../docs/adr/0062-deferred-render-path.md).

Today `RenderSystem::tickVulkan` calls `ForwardRenderPath::renderFrame` (shadow, then one CLEAR color+depth pass: opaque / scene overlay / transparent secondaries). SSAO, outline, overlay lines, overlay AA, and screen overlays consume that offscreen. Camera Preview / Mesh Preview call `renderFrameTo`. Lighting is per-draw `gatherLightsForMesh` (cap 8, Light linking) into `pbr.slang` / `pbr_skinned.slang`. Frame graph Execute does not resolve RHI or record GPU work.

## Goals / Non-Goals

**Goals:**

- Hardcoded `DeferredRenderPath` the editor viewport can select.
- G-buffer extra textures + shared offscreen depth; lighting writes offscreen color.
- CPU Deferred light list (32) + per-receiver 8 that matches `gatherLightsForMesh`; first-party test without a window for story 5.
- Secondary slots for G-buffer and lighting; reuse overlay / SSAO / pick / shadow map.

**Non-Goals:**

- Frame graph GPU; clustered lighting; visbuffer; HDR; product UI; raising 8; Point/Spot/Area shadows; D3D12 deferred.

## Decisions

1. **`DeferredRenderPath` beside `ForwardRenderPath`**  
   `RenderSystem` branches the editor viewport on `BLUNDER_EDITOR_DEFERRED` (same getenv style as `BLUNDER_EDITOR_SHADOWS`). Previews keep `renderFrameTo`.  
   *Alternatives:* Frame graph GPU (Grill A rejected); replace Forward in place (rejected).

2. **G-buffer is path-owned extra images, not OffscreenRenderTarget MRT**  
   Three attachments + shared offscreen depth: `RGBA8` albedo+AO; `RGBA8` oct-normal.rg + metallic + roughness; `R16_UINT` receiver (draw slot 0–255, unlit, two-sided; clear to a no-geometry sentinel). Double-buffer with `k_buffer_count`. Geometry does not write offscreen color. Lighting is a fullscreen triangle: invalid receiver → viewport background; else reconstruct world pos from depth and shade.  
   *Alternatives:* permanent MRT on offscreen (rejected); blit a deferred-only color (rejected).

3. **Receiver id is the opaque draw slot, not EntityId or pick ID**  
   Slot matches the Forward mesh draw cap (256). CPU builds a MeshRenderer EntityId table for those slots and a Deferred light list (32). Lighting applies linking + 8 using that id. Shared helpers with the first-party test.  
   *Alternatives:* store EntityId bits (awkward GPU linking); reuse pick ID (leaf, different pass).

4. **Post-lighting LOAD pass for scene overlay + transparent**  
   Existing offscreen RP CLEARs. After lighting, begin the offscreen with LOAD color+depth, execute `forward_scene_overlay` then `forward_transparent` (Forward PBR, Bindless, same gather as today). New SecondaryPass: `gbuffer_opaque`, `deferred_lighting`. Barriers on PRIMARY.  
   *Alternatives:* scene overlay after transparent (wrong grid composite); put overlays in the G-buffer pass (no color yet).

5. **Shadow and Bindless unchanged in role**  
   Shadow pass still before geometry when editor shadows are on. Lighting SampleCmp the same map (dedicated binding, not Bindless). G-buffer geometry binds the Bindless table like Forward opaque; lighting does not. Skinned G-buffer shader consumes the same Matrix Palette UBO.  
   *Alternatives:* skip shadows on deferred (rejected); a second shadow map.

6. **Engine GPU cache follows shader bytes**  
   New `gbuffer.slang` / `gbuffer_skinned.slang` / `deferred_lighting.slang` (names may match repo style). Do not bump `k_engine_gpu_cache_generation` unless extract identity changes without touching those bytes. Exact-match FATAL on each new pipeline’s Shader resource layout.  
   *Alternatives:* reuse `pbr.slang` with ifdef (fights MRT vs lighting).

## Risks / Trade-offs

- [Upload cap 32 drops a light that only linking-receivers would have seen] → Documented; evaluation cap stays 8; test the 33rd drop. SSBO later.
- [Sharing offscreen depth across G-buffer RP and LOAD overlay RP] → PRIMARY barriers; do not put layout changes in continue secondaries (ADR 0059).
- [Unlit / two-sided packed in R16] → CPU mirror in the light-list test is not required; visual story 2 covers unlit.
- [LOAD offscreen RP does not exist today] → Add a LOAD begin path; do not CLEAR after lighting.
- [Zero-copy presents offscreen color] → Lighting writes that image; G-buffer is never presented.

## Migration Plan

1. CPU light-list helpers + `deferred_light_list_test` (or extend `light_eval_test`) for story 5.
2. G-buffer targets + shaders + `DeferredRenderPath`; viewport branch; SecondaryPass; LOAD overlay/transparent; render-pipeline.md + CONTEXT already grilled.
3. Default remains Forward. Rollback: revert the path; env unset is Forward.

## Open Questions

None.
