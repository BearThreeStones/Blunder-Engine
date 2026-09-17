## Why

The viewport still CPU-assembles every mesh draw (cap 256). Sponza-scale static geometry cannot submit or cull at Meshlet grain, so frustum, back-face, and occlusion stay coarse and CPU-bound. Bindless already said this table is not GPU-driven rendering; this change is that geometry path.

## What Changes

- Add **GPU-driven rendering** as the default geometry submission for static opaque and alpha-clip MeshRenderers in the editor viewport and Player: cook-time Meshlets, GPU frustum + cone cull, previous-frame Hi-Z with an early/late pair, GPU-written indirect commands.
- Task/mesh shaders when `VK_EXT_mesh_shader` is present; otherwise compute cull plus traditional vertex/fragment indirect draws.
- Directional shadow pass uses the same GPU-driven static geometry.
- Shading stays on the existing Forward or Deferred Render Path (not a visibility buffer).
- Skinned, blend-transparent, pick, outline, Mesh Preview, Camera Preview, and Scene Thumbnail stay on the CPU draw list (Forward mesh draw cap still 256 there).
- Cooked mesh Finals gain Meshlet payloads (MeshOptimizer at Cook). Fast Path without Meshlets does not take the GPU-driven path.
- Test project gets `Assets/Scenes/sponza.scene.asset` using the existing Sponza mesh. Crytek resources are **not** copied into the engine repo.

## User stories

1. I open the Test project’s `sponza.scene.asset` and the atrium is in the viewport (existing Sponza mesh, a camera, and a directional light). The engine repo does not contain a copy of the Crytek files.
2. I orbit the atrium: Meshlets that leave the frustum or face away are not submitted; the hall in front of me is not missing chunks.
3. I stand behind a pillar: occluded geometry is Hi-Z culled; when I step out, the late pass restores it without a frame of holes.
4. On a GPU without mesh shaders, Sponza still draws through compute cull plus vertex/fragment indirect draws — not a black viewport.
5. I open a small scene with a skinned character: skinning, gizmos, pick, and outline still use the CPU path; static props in that scene still use GPU-driven rendering.
6. I Play the Sponza scene: the directional light’s shadow covers the visible atrium, not a 256-draw truncated remnant.

## Capabilities

### New Capabilities

- `gpu-driven-rendering`: Default static opaque/alpha-clip geometry path: Meshlets, GPU cull (frustum, cone, previous-frame Hi-Z early/late), mesh shaders or compute+VS/FS indirect, Directional shadows, Sponza demo scene in the Test project.

### Modified Capabilities

- `deferred-render-path`: GPU-driven static opaque writes the G-buffer; G-buffer receiver id is the MeshRenderer (Meshlets inherit it) and MAY exceed the CPU Forward mesh draw cap.
- `bindless-texture-table`: Forward mesh draw cap remains 256 for CPU lists only; GPU-driven static opaque is not truncated by that cap.

## Impact

- **Engine:** mesh Cook / `.meshbin`; RHI compute, indirect draw, optional `VK_EXT_mesh_shader`; new Slang (cull, Hi-Z, task/mesh or VS); Forward/Deferred record paths; Frame graph callbacks host cull + pyramid (path-owned images, not visbuffer).
- **Dependencies:** MeshOptimizer via FetchContent (not vendored under `engine/3rdparty/` except the Slint fork exception).
- **Hosts:** Editor viewport and Player. Preview/Thumbnail/pick unchanged in contract.
- **Content:** Test project Sponza scene only; no Crytek tree in Blunder-Engine.
- **Docs:** CONTEXT GPU-driven / Meshlet / Hi-Z (grilled); [ADR 0070](../../../docs/adr/0070-gpu-driven-rendering.md); render-pipeline.md.
- **Tests:** Meshlet cook + frustum/cone math + indirect command layout as first-party tests. Merge CI has no GPU Sponza run.
