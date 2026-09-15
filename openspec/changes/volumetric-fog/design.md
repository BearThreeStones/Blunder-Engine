## Context

See `proposal.md` for motivation. `RenderSystem` already records `ForwardRenderPath` for both editor Viewport and Player (`EngineHostMode::Player`), then optional SSAO (currently forced off) and present/readback. Player has no clustered light list on `main`. There is no fog system. Compute exists (`pick_broad_phase.slang`). Fullscreen composite exists (`SsaOPass`). Scene lights are Unique Light Components; at most one Directional casts shadows. World space is Z-up.

Grill lock: Player Forward composite `color * T + inscatter`; Viewport may skip fog; 16 px × 64 exponential Z; Z-up height density; no 2D height-fog pass; unshadowed directional; thin temporal (~20% current); no volumetric shadows; no point/spot inject; stack on Forward; no GBuffer; propose now, apply later.

Do not amend `mesh-shader-shadows` or archived clustered-deferred / gpu-driven changes.

## Goals / Non-Goals

**Goals:**

- Fog Unique + Player-only froxel volume + unshadowed directional scatter + thin temporal + fullscreen Forward composite.
- Hand-written compute/composite layouts (SSAO/pick class), 3D textures off the Bindless color table.
- CPU-testable Z mapping, height density, host gate, and Fog pick — GPU apply on the Windows worker when that tree is free.

**Non-Goals:**

- Editor Viewport / Camera Preview / Placement Preview / Mesh Preview / Scene Thumbnail fog.
- Volumetric shadows, local-light inject, clustered lists, GBuffer, analytical 2D height fog, box fog, Local Fog Volumes, voxelized fog meshes, 5×5 spatial, 64³ Perlin bake, `max()` history, opacity-AA, in-mesh-shader fog samples.
- Hierarchy **Create… Fog** (Add… only). New Scene does not spawn Fog.
- Changing Light Component fields (no per-light volumetric intensity this slice).

## Decisions

1. **Player gate, not a second renderer** — Run volume + composite only when `hostMode() == Player` and an active Fog exists. Same `ForwardRenderPath` as today. Viewport stays unfogged without a parallel deferred path.

   *Alternative:* in-shader lookup in every forward mesh (Packt / Unreal forward base pass). Rejected: Grill asked for a Forward **composite**; one fullscreen pass matches SSAO and leaves mesh shaders alone.

2. **Fullscreen composite after Forward** — After `ForwardRenderPath::renderFrame` writes color+depth, dispatch volume computes (or overlap if barriers allow), then `color * T + inscatter` like `SsaOPass::apply` (scene snapshot if the composite cannot sample the color attachment in-place). Sky: reconstruct far view-Z from the depth clear so empty pixels still sample the last slice.

   *Alternative:* build volume before the base pass and sample inside `pbr.slang`. Rejected this slice (Decision 1). Transparencies already in the color buffer are fogged at their written depth — acceptable.

3. **Unreal-sized froxels, not Packt 128³** — `ceil(viewExtent / 16) × 64` RGBA16F. Exponential Z with Unreal `CalculateGridZParams` family, **S = 32**, near = `max(playNear, 9.5 cm)`, far = Fog view distance (default 60 m), full 64 slices. Resource grid may follow the offscreen extent so resize is stable.

   *Alternative:* fixed 128³ or Wronski 160×90×64. Grill locked ~16 px × 64 Z.

4. **Density is volumetric-only height fog** — `ρ · exp2(−falloff · (world.z − fogHeight))` with `fogHeight` = Fog entity world Z. No `MatchHeightFogFactor` (that exists to blend with a 2D pass we are not shipping). No Y-up. No box AABB term. No 7-octave noise.

5. **Fog Unique as enable + authoring** — Optional `"fog"` on the entity, parallel to `"light"` / `"camera"`. Missing Fog ⇒ skip the pass (DogWalk stays clear). Defaults match Unreal-visible height fog (density 0.02, falloff 0.2, 60 m, `g` 0.2, white albedo) so Add… Fog is immediately visible in Player. First matching Fog in EntityId order.

   *Alternative:* hardcoded engine constants. Rejected: always-on fog would hit every Play scene.

6. **One unshadowed Directional** — First Light enabled Directional whose contribution includes illumination, EntityId order (not the shadow-caster pick, which can be Shadows-only). `ShadowFactor = 1`. Phase: Henyey–Greenstein, `g` from Fog. No hidden ambient (scene lights already forbid a fill). No locals: Player has no clustered list; do not build a fog-only Light Grid.

7. **Thin temporal, history on scatter** — Ping-pong RGBA16F 3D on the **pre-integrated** lighting (Unreal `LightScattering` / Packt scatter). `mix(history, current, 0.2)`. Reproject with previous view-projection; skip blend when UVW is outside `[0,1]`. Halton XY jitter when temporal is on. Integrate **after** temporal (Hillaire / Frostbite slice integral). Skip `max(history, current)` and 5×5 Gaussian.

   *Alternative:* skip temporal (Unreal brief “slice 1”). Rejected: Grill locked thin temporal at 64 Z.

8. **Pass order (Player)** — Forward opaque+transparent → density CS → scatter CS → temporal → integrate CS (8×8×1, loop Z) → composite FS → present. Scatter groups 4³. New `VolumetricFogPass` under `runtime/function/render/` (or `render/post/`), Slang CS + FS, hand-written layouts.

9. **Apply placement** — This PR is planning only. `/opsx:apply` waits until the Windows dirty GPU tree is free. Do not land engine/GPU code beside in-flight mesh-shader shadows. Linux/cloud may land serializer + CPU tests first if that does not touch the dirty render tree.

10. **Glossary** — Add Fog Component / volumetric fog / volumetric shadows (out of scope) to `CONTEXT.md` in this change.

## Risks / Trade-offs

- **[Risk] Transparent fogged as opaque-at-depth → Mitigation:** Accept this slice; in-shader volume sample for translucency is later.
- **[Risk] Camera far ≫ 60 m leaves distant geometry unfogged → Mitigation:** Fog view distance is the volume far; document it; no 2D height-fog backdrop this slice.
- **[Risk] No Directional ⇒ extinction without in-scatter (dark fog) → Mitigation:** Spec-correct; do not add a hidden ambient.
- **[Risk] Temporal ghosting on moving Fog/camera → Mitigation:** Expected at 0.2 current; density is height+constant so trails stay mild.
- **[Risk] Reverse-Z / Vulkan Y-flip reconstructs wrong view Z → Mitigation:** CPU unit tests for slice ↔ depth; match Play camera near/far, not Editor Camera.
- **[Risk] Touching Forward mesh shaders fights mesh-shader-shadows → Mitigation:** Composite-only; no `pbr.slang` fog sample.

## Migration Plan

1. Optional `"fog"` JSON; old scenes load with no Fog (unfogged Player).
2. Add… Fog opts a scene in. No New Scene Fog entity.
3. Rollback: leave Fog Components in the document; skip the Player pass.
4. Later slices (not this change): Viewport fog, local inject once Player has a clustered list, volumetric shadows.

## Open Questions

None — Grill confirmed the four User stories. Exact Slang file names and whether density+scatter share one UAV are apply-time.
