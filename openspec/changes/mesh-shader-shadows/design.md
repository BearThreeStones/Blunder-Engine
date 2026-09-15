## Context

See `proposal.md` for motivation. Cloud `main` still has one 1024² `D32` directional ortho map, VS/FS `shadow_depth.slang`, 2×2 `SampleCmp` PCF, and no meshlets. Cooked GPU-driven meshlets and a deferred GBuffer live on the Windows tree (archived `gpu-driven-rendering`, not this change). Clustered deferred is a sibling OpenSpec (`clustered-deferred-local-lights`) for Viewport froxel lighting only.

Two research briefs informed this design (read-only):

- **Unreal 5.7.4 brief** (`unreal-mesh-shader-shadows-brief.md`): four systems share “mesh shader / Nanite / virtual / cluster” language. They are **not** one pass.
- **Packt Ch. 8 brief** (`mesh-shader-shadows-paper-brief.md`): NV mesh/task cubemap shadows for point lights. Grill **overrode** its “points only / no directional VSM” product limit. Keep its task-cull + mesh depth-emit idea, layered `gl_Layer`, and opaque-meshlet caster set; ignore its 256-light sparse pool, Vogel software PCF, and tetrahedron atlas.

### Unreal split (do not collapse)

| | Unreal meaning | This slice |
|---|---|---|
| **Mesh shaders** | Nanite **hardware triangle raster** (`HWRasterizeMS`). `UseMeshShader()` ignores `EPipeline` — BasePass and Shadows share it. **Not** a VSM pass. Amplification is none; compute culls clusters. Cubemap Nanite path is **six 2D visbuffer rasters + `EmitCubemapShadow`**, because the visbuffer is not layered. | **Copy the raster idea only:** EXT mesh/task shaders emit **depth-only** triangles from **Blunder cooked meshlets** into real targets (VSM physical pages, cube faces, spot 2D). **Skip** Nanite visbuffer, SW raster, `EmitShadowMap` / `EmitCubemapShadow`, cluster DAG, 6-pass visbuffer cubemap. |
| **VSM** | **Virtualization:** 128² pages × 128² level-0 = 16k virtual, physical page pool (default `MaxPhysicalPages` 2048), camera-centered **clipmaps** (`FirstLevel` 6 … `LastLevel` 22, radius `2^(level+1)`), pixel / optional **`Froxel::` occupancy** page mark, **SMRT** filter. Code default `r.Shadow.Virtual.Enable` 0. **Not** a mesh-shader pass. Local lights get 1 (spot) or 6 (point) virtual maps. | **Honor Grill anyway:** directional **clipmap + page table + physical pool**, **filled by Blunder mesh shaders**. Page mark from **GBuffer depth pixels**, not `Froxel::`. Sample with **page-table lookup + comparison/PCF**, not SMRT. **No** local-light VSM pages this slice. |
| **Classic maps** | Directional CSM atlas (up to 10 cascades); spot perspective 2D; point **one-pass layered cubemap** (`ONEPASS_POINTLIGHT_SHADOW`, VS writes `SV_RenderTargetArrayIndex`). | Point + spot use **classic real maps** (cube / 2D), not VSM. Directional does **not** take CSM cascade count. |

**Do not claim UE VSM is a mesh-shader pass.** Unreal VSM is page mark + virtualization; Unreal mesh shaders are Nanite HW raster that *can* target VSM physical pages via `VIRTUAL_TEXTURE_TARGET`. Blunder composes those two Unreal pieces on purpose: clipmap pages **filled by** mesh shaders of **our** meshlets.

## Goals / Non-Goals

**Goals:**
- One shared mesh/task depth-only raster over opaque cooked meshlets for Viewport and Player.
- Directional: Unreal-shaped VSM/clipmap, mesh-filled physical pages.
- Point: 6-face cubemap, layered dispatch if `shaderOutputLayer` from mesh shaders is available, else 6 passes; existing PCF.
- Spot: one 2D perspective map; existing PCF.
- VS/FS `shadow_depth` fallback when mesh shaders are missing (classic 1024² directional + VS-filled cube/spot — **not** a second VSM implementation).

**Non-Goals:**
- New meshlet cook or amending archived `gpu-driven-rendering`.
- Amending `clustered-deferred-local-lights`; no froxel-list ↔ shadow-page binding.
- Nanite visbuffer, 6-pass cubemap emit, SW cluster raster, `Froxel::` occupancy.
- Local-light VSM, SMRT, CSM cascade count, tetrahedron atlas, transparent/alpha/skinned/foliage casters.
- Camera Preview / Mesh Preview / Placement Preview / Scene Thumbnail shadows (Camera Preview stays off, ADR 0018).

## Decisions

**Compose Unreal VSM virtualization with Blunder mesh-shader fill; do not copy Nanite.** Grill locked “directional VSM/clipmap pages filled by mesh shaders.” Unreal implements those as two systems. Blunder’s clipmap/page-table/physical-pool is the VSM half; Blunder’s EXT mesh/task depth-only raster of cooked meshlets is the fill half. Nanite visbuffer + emit is skipped, including the 6-pass cubemap.

**Clipmaps, not CSM.** Camera-centered directional clipmaps whose world radius doubles per absolute level (`2^(level+1)`), matching Unreal `FVirtualShadowMapClipmap`. Page size **128²**, virtual level-0 **128×128 pages** (16k), physical pool (not a 16k texture). FirstLevel **6** (Unreal default). LastLevel **10** this slice (five levels, far radius 2¹¹ = 2048) — enough for Blunder’s current ~60-unit directional far without shipping Unreal’s 6…22 (17-level) default. Raising LastLevel later is still clipmaps. **Do not** add `r.Shadow.CSM.MaxCascades` or pack ortho cascades into an atlas.

**Page mark from GBuffer depth pixels, not `Froxel::`.** Unreal can mark pages from pixels (`GeneratePageFlagsFromPixels`) or froxels (`r.Shadow.Virtual.MarkPagesUsingFroxels`, default 0). Grill skipped Nanite/`Froxel::` occupancy. This slice marks pages by projecting GBuffer depth (Viewport/Player view) into clipmap UV and requesting those pages. Coarse-page mark (Unreal levels 15…18) is skipped because LastLevel is 10.

**Directional sample is page-table + PCF, not SMRT.** Grill locked existing PCF for point cubes and spot 2D. It did not lock Unreal SMRT (7 rays × 8 samples). Keep one filtering story: 2×2 hardware `SampleCmp` on the comparison view of the **physical page** after page-table translate. Missing/unmarked pages shade as unshadowed (1.0), same as today’s border-white sampler.

**Local lights stay on classic maps, not VSM.** Grill: do not put local lights into VSM this slice. Each shadowing Point gets a cubemap (or 6-layer 2D array); each shadowing Spot gets one 2D perspective map. Physical VSM pool is directional-only.

**Point fill: layered mesh dispatch, not Nanite 6-pass visbuffer.** Prefer Unreal **classic** one-pass layered cube (`gl_Layer` / `SV_RenderTargetArrayIndex` from the mesh shader) when the device exposes mesh shaders **and** `shaderOutputLayer`. If layered mesh output is missing, six sequential mesh (or fallback VS) passes, one face each — still real cube faces, still not visbuffer emit. Packt’s “six indirect commands + one layered FB” is an acceptable implementation of the same idea. Skip `GL_EXT_multiview`.

**Task shader culls; mesh shader emits depth-only.** Map Packt NV task/mesh onto `VK_EXT_mesh_shader` (`EmitMeshTasksEXT` / `SetMeshOutputsEXT`), local size 32, max 64 verts / 124 prims to match typical meshoptimizer meshlets. Task: cone + sphere + (clipmap-page frustum | cube-face mask | spot frustum). Mesh: positions only, no fragment shader on the meshlet technique. Unreal Nanite’s “compute cull, no amplification” is the alternative; declined because Blunder already has meshlet cone/sphere records and does not have Nanite’s cull pipeline.

**VS/FS fallback is classic maps, not VSM-in-vertex-shaders.** If `VK_EXT_mesh_shader` (or meshShader feature) is absent, keep today’s 1024² directional ortho + VS `shadow_depth` / `_skinned` for that map, and fill point cubes / spot 2D with the same VS path. Do not implement page-table raster in vertex shaders this slice.

**Casters = opaque cooked meshlets only.** Instance-list build skips alpha-mask, transparent, skinned, and foliage. Skinned `shadow_depth_skinned` is not extended to meshlets. Linking still applies: a light with a non-empty receiver list only receives shadows from / casts onto those MeshRenderers as today’s linking rule (illumination and/or that light’s shadows).

**Viewport product-on, same recording as Player.** `BLUNDER_EDITOR_SHADOWS` is no longer the product gate (it hid Viewport shadows by default to avoid doubling forward draws). Viewport and Player both run the pass when a shadowing light exists. Camera Preview stays `shadows_enabled = false` so it does not rewrite shared maps (ADR 0018). Mesh Preview / Studio stay off.

**Do not bind clustered light lists to shadow pages.** Clustered deferred fills froxels for *lighting*. Shadow page mark, cube allocation, and spot maps are driven by the shadowing Light Components and GBuffer depth, not by `froxelLightIndices`. When clustered later shades point/spot, it may *sample* these maps; that wiring is not this change.

**Shadow-map budget for locals.** At most **8** shadowing Point lights and **8** shadowing Spot lights per view (first Light-enabled, contribution includes shadows, Active in Hierarchy, stable EntityId). Extra casters are dropped with a log. Directional remains at most one. Distinct from the illumination 8-cap (clustered Viewport may illuminate more than 8; they still do not all get maps).

**Bindless stays color-only.** Directional page table + physical pages, point cubemaps, and spot 2D maps are dedicated shadow bindings (comparison and/or page-table UAV/SRV), never Bindless color-table entries (ADR 0056).

**Physical pool size.** Default **512** physical 128² `D32` pages for the one directional clipmap (64 MiB depth), below Unreal’s 2048 because this slice has one VSM light. Overflow (more marked pages than pool) drops farthest/coarsest pages and logs; no silent wrap.

## Risks / Trade-offs

- **[Risk]** Cloud `main` has no meshlets; apply on a tree without GPU-driven cook will have an empty caster set. → **Mitigation:** this change does not recook. Apply after the Windows GPU-driven meshlet buffers exist. Tasks start by confirming those buffers’ layout (center, radius, cone, verts, indices) rather than assuming `main`.
- **[Risk]** Mesh-shader `shaderOutputLayer` missing on some GPUs that still have mesh shaders. → **Mitigation:** spec already allows 6 face passes; detect `shaderOutputLayer` at device init.
- **[Risk]** LastLevel 10 / 512 pages under-cover a huge scene or overflow the pool. → **Mitigation:** log overflow; LastLevel and pool size are constants this slice, not CSM. Fast-follow can raise them without changing topology.
- **[Risk]** Pixel page-mark from GBuffer misses casters that shadow off-screen receivers. → **Mitigation:** accepted for VSM (Unreal marks from visible receivers too). Off-screen shadowing onto visible pixels still works if those pixels mark the page; casters are still frustum/page culled per marked page.
- **[Risk]** Clustered Viewport lighting (sibling change) will not automatically sample the new point/spot maps. → **Mitigation:** explicit non-goal; document in tasks that clustered sampling is a later change. Forward/deferred lighting sites this slice already owns should sample.
- **[Risk]** Fallback VS path + VSM mesh path diverge visually. → **Mitigation:** fallback keeps the *old* directional 1024² look, not a fake clipmap. Document as a capability difference, not a bug.

## Migration Plan

Additive GPU resources and a new shadow pass permutation. No scene/asset format change; no new cook. Until meshlets exist, the pass records an empty caster list and existing VS shadows remain for fallback devices. Rollback is not merging / not enabling the mesh-shader pipelines. Glossary **Light shadows** in `CONTEXT.md` updates when this change archives.

## Open Questions

- Exact Viewport stat / log string for VSM page-pool overflow and dropped 9th local caster is an implementation detail for `tasks.md`.
- Whether `BLUNDER_EDITOR_SHADOWS` remains as a debug *off* switch after the product gate is removed can be decided at apply without changing specs (default is on when casters exist).
