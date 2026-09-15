# Manual checklist — Mesh-shader shadows

Human-run acceptance for the `mesh-shader-shadows` change. Requires cooked GPU-driven opaque meshlets on the apply tree (Windows GPU-driven work). Cloud `main` may not be enough.

**Status: Not run.**

| # | User story | Acceptance check | Status |
|---|---|---|---|
| 1 | Viewport and Player show mesh-shader shadows. Opaque meshlets only. VS/FS fallback if mesh shaders are missing. No alpha/skinned/foliage casters. | On a mesh-shader GPU, open a scene with opaque meshlets and a shadowing Directional: both editor Viewport and Player show occluder shadows. Hide/replace with only alpha, skinned, or foliage: those do not cast. On a device without mesh shaders (or a forced fallback), classic VS/FS shadows still appear; the view is not blank. | Not run |
| 2 | Directional uses Unreal-style VSM/clipmap pages filled by mesh shaders. No classic CSM count. No local lights in VSM. | Confirm (capture/stats) the Directional writes clipmap physical pages, not a CSM cascade atlas. Add shadowing Point/Spot lights: they do not consume directional VSM pages. | Not run |
| 3 | Point lights: 6-face cubemap, layered mesh dispatch if Vulkan allows else 6 passes. Existing PCF. | Place an opaque occluder between a shadowing Point and a receiver inside range: Viewport and Player show cube shadows. Capture shows six faces filled (one layered draw or six passes), not a Nanite visbuffer emit. Filtering matches existing PCF (no SMRT/Vogel look). | Not run |
| 4 | Spot lights: one 2D depth map, same meshlet depth-only path, existing PCF. | Place an opaque occluder inside a shadowing Spot cone: Viewport and Player show that 2D occlusion. Confirm a single 2D map per spot (not a cube, not VSM). | Not run |
| 5 | Stack on existing cooked GPU-driven meshlets. Clustered deferred stays separate; do not bind clustered light lists to shadow pages. | Confirm no new meshlet cook ran for this change. If clustered froxels exist, page mark / cube fill still do not read `froxelLightIndices`. | Not run |

## Regression checks

- [ ] Camera Preview stays shadows-off and does not rewrite Viewport maps (ADR 0018).
- [ ] Mesh Preview / Placement Preview / Scene Thumbnail do not pick up this mesh-shader shadow pass.
- [ ] Area Lights still do not cast.
- [ ] At most one shadowing Directional (first EntityId).
- [ ] Bindless color table still has no shadow resources.
- [ ] `clustered-deferred-local-lights` artifacts were not edited.
