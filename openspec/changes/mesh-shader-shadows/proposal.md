## Why

Viewport and Player still shade from one classic 1024² directional ortho map filled by VS/FS triangle draws. Point and Spot lights never cast. GPU-driven opaque meshlets (uncommitted on the Windows tree) already exist as the caster set this slice must use; without a mesh-shader shadow pass, those meshlets cannot produce occlusion, and local lights stay unshadowed. This change gives Viewport and Player mesh-shader shadows for the one shadowing Directional (Unreal-style VSM/clipmap pages), Point cubemaps, and Spot 2D maps, stacked on the existing cooked meshlets — not a new cook, not clustered lighting, and not Nanite.

## What Changes

- Fill shadow depth from **opaque GPU-driven meshlets** with a mesh/task depth-only path (`VK_EXT_mesh_shader`). Alpha, skinned, and foliage meshlets are not casters this slice. When the device lacks mesh shaders, keep the existing VS/FS `shadow_depth` path as the fallback.
- Replace the single classic directional ortho map with **Unreal-style VSM/clipmap pages** for the one shadowing Directional (first Light-enabled Directional whose contribution includes shadows, stable EntityId order). Mesh shaders rasterize opaque meshlets **into those physical pages**. Do **not** add a classic CSM cascade count. Do **not** put Point or Spot lights into VSM this slice.
- Give each shadowing **Point** light a **6-face cubemap**. Use one layered mesh dispatch when Vulkan can write `gl_Layer` / `SV_RenderTargetArrayIndex` from the mesh shader; otherwise six face passes. Sample with the existing 2×2 hardware PCF (not Unreal SMRT, not Packt Vogel).
- Give each shadowing **Spot** light **one 2D perspective depth map**, filled by the same meshlet depth-only path, sampled with the existing PCF.
- Viewport **and** Player share the same caster recording and maps. Camera Preview, Mesh Preview, Placement Preview, and Scene Thumbnail stay off this slice (Camera Preview remains shadows-off per ADR 0018).
- Stack on the **existing cooked GPU-driven meshlet buffers**. Do not propose a new meshlet cook. Clustered deferred stays a separate change: do not bind froxel / clustered light lists to shadow pages.

Out of scope: Nanite visbuffer / 6-pass cubemap emit, `Froxel::` occupancy page-mark, local-light VSM pages, transparent / alpha / skinned / foliage casters, SMRT, tetrahedron atlases, Packt 256-light sparse cubes.

## User stories

1. I open a scene in the editor Viewport and in the Player and see mesh-shader shadows on both. Only opaque meshlets cast. If the GPU has no mesh shaders, I still see the old VS/FS shadows instead of a blank or crashed view. Alpha, skinned, and foliage do not cast.
2. I light the scene with a Directional and see Unreal-style VSM/clipmap pages filled by those mesh shaders — not a stack of classic CSM cascades, and not local lights packed into that virtual map.
3. I add Point lights whose contribution includes shadows and see 6-face cubemap occlusion, filled in one layered mesh dispatch when Vulkan allows it, otherwise six passes, still using the existing PCF.
4. I add Spot lights whose contribution includes shadows and see one 2D depth map per spot, filled by the same meshlet depth-only path, still using the existing PCF.
5. I keep the cooked GPU-driven meshlets I already have; this change does not recook clusters. Clustered deferred stays a different change — shadow pages are not wired to clustered light lists.

## Capabilities

### New Capabilities
- `mesh-shader-shadow-raster`: depth-only mesh/task fill of shadow targets from opaque cooked GPU-driven meshlets for Viewport and Player; VS/FS `shadow_depth` fallback when `VK_EXT_mesh_shader` is missing; no alpha/skinned/foliage casters.
- `directional-virtual-shadow-map`: Unreal-style VSM/clipmap page table + physical pool for the one shadowing Directional; pages filled by the mesh-shader raster (Blunder meshlets, not Nanite visbuffer); no classic CSM count; no local lights in VSM.
- `point-cubemap-shadows`: 6-face cubemap per shadowing Point light; layered mesh dispatch when Vulkan allows, else 6 passes; existing PCF; skip Nanite 6-pass visbuffer emit.
- `spot-perspective-shadows`: one 2D perspective depth map per shadowing Spot light; same meshlet depth-only fill; existing PCF.

### Modified Capabilities
- `scene-light-component`: "Light shadows this slice" expands from Directional-only to Directional (VSM/clipmap) + Point (cubemap) + Spot (2D). Area still does not cast. At most one shadowing Directional remains. Shadows-only on Point/Spot becomes a working product path.
- `bindless-texture-table`: shadow resources stay dedicated comparison / page-table bindings (directional VSM pages, point cubemaps, spot 2D maps) and SHALL NOT enter the color Bindless table.

## Impact

- Render: RHI mesh-shader pipelines, layered cubemap depth targets, directional VSM page table + physical page pool, per-spot 2D maps; `ForwardRenderPath` / deferred frame-graph shadow pass (whichever the Windows GPU-driven tree exposes); `shadow_depth.slang` fallback retained.
- Scene: caster pick in `light_eval` / `CONTEXT.md` **Light shadows** glossary — Point and Spot with contribution including shadows now cast; Area still does not.
- Sampling: existing 2×2 `SampleCmp` PCF for cube and spot; directional samples resident clipmap pages (comparison/PCF on the physical pool, **not** Unreal SMRT).
- Prerequisites (external, not this change): cooked GPU-driven meshlets + deferred GBuffer on the Windows tree. Cloud `main` may look incomplete. Do not amend `clustered-deferred-local-lights` or archived `gpu-driven-rendering`.
- Docs: `CONTEXT.md` Light shadows; `docs/agents/render-pipeline.md` shadow pass notes.
- Non-goals: new meshlet cook, Nanite, `Froxel::` occupancy, local-light VSM, transparent shadows, clustered-light-list coupling.
