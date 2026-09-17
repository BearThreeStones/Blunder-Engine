## Context

See proposal.md for why. Grill locked: full chapter (Meshlets, compute frustum/cone, indirect, previous-frame Hi-Z early/late, `VK_EXT_mesh_shader` with compute+VS/FS fallback); static opaque default (Unreal GPU Scene analog); existing Forward/Deferred shading, not visbuffer; Test-project Sponza scene only; Directional shadows included. Domain: [CONTEXT.md — GPU-driven rendering](../../../CONTEXT.md). Decision: [ADR 0070](../../../docs/adr/0070-gpu-driven-rendering.md).

Today `RenderSystem` builds `ForwardOpaqueDraw` on the CPU (cap 256) and records one `vkCmdDrawIndexed` per draw. Compute exists for pick broad-phase on the graphics queue. No indirect draws, no mesh shaders, no MeshOptimizer. Mesh Cook Final is version 2 (skin flags). G-buffer receiver id packs an 8-bit draw slot. `GltfSceneImporter` creates one entity per mesh primitive. Timeline semaphore ADR forbids a dedicated compute queue in that slice.

## Goals / Non-Goals

**Goals:**

- Cook-time Meshlets in `.meshbin` (version 3) via MeshOptimizer FetchContent.
- Per-frame GPU instance buffer (MeshRenderer world matrix, bindless indices, meshlet range, receiver id).
- Graphics-queue compute: early cull, late cull, Hi-Z pyramid from previous offscreen depth.
- Mesh/task pipelines when `VK_EXT_mesh_shader`; else `vkCmdDrawIndexedIndirectCount` + existing PBR/G-buffer fragment contract.
- Path-owned Hi-Z images (like G-buffer), not Frame graph Transients.
- Widen G-buffer receiver to a MeshRenderer slot that fits Sponza-scale static counts.
- First-party tests for Meshlet cook, cone/frustum math, and indirect command layout.

**Non-Goals:**

- Visibility buffer / Nanite software raster / LOD DAG.
- Skinned Meshlets; GPU-driven transparent; GPU-driven pick/outline/Preview/Thumbnail.
- Dedicated compute queue; `VK_NV_mesh_shader`.
- Vendoring Crytek files into Blunder-Engine.
- Raising the CPU Forward mesh draw cap for skinned/transparent.

## Decisions

1. **GPU Scene analog, not a scene flag**  
   CPU uploads dirty MeshRenderer records; GPU culls Meshlets and writes draw commands. Default for static opaque/alpha-clip when Meshlets exist.  
   *Alternatives:* Sponza-only opt-in (Grill rejected); replace skinned/Preview (Grill rejected).

2. **Meshlets live in Cook Final, version 3**  
   `meshopt_buildMeshlets` (64 verts, 124 tris) plus `meshopt_computeMeshletBounds`. Payload after today’s vertex/index/skin. Fast Path and version ≤2 Finals have no Meshlets → CPU list. Recook required. MeshOptimizer is FetchContent, not `engine/3rdparty/` (Path protection).  
   *Alternatives:* build Meshlets on load (Grill: not on the tick); vendor the library under 3rdparty (denied except Slint).

3. **`VK_EXT_mesh_shader` then compute+VS/FS**  
   Query the extension at device create; do not fail init if missing. Mesh shaders replace the vertex stage only; fragment stays PBR / G-buffer with Bindless set 1. Compute fallback writes `VkDrawIndexedIndirectCommand` (or equivalent) from Meshlet local indices expanded to the mesh index buffer. Exact-match FATAL on every new pipeline.  
   *Alternatives:* require mesh shaders (Grill rejected); NV-only (book is outdated).

4. **Previous-frame Hi-Z, path-owned**  
   After the late opaque (and after shadow is done), downsample offscreen depth into a pyramid sampled next frame. Early pass: frustum + cone + Hi-Z. Draw. Late pass: retest Hi-Z rejects against the updated depth, draw. First frame skips Hi-Z. SSAO’s depth read is unrelated. Do not import the pyramid as Frame graph resources (same reason as G-buffer extras, ADR 0069).  
   *Alternatives:* depth prepass (Grill rejected); Frame graph Transients for the pyramid (format/FIF ownership already on the path).

5. **Shadows: GPU frustum/cone, no Hi-Z**  
   Reuse Meshlet data with the Directional light VP. One indirect shadow draw path for GPU-driven static. CPU list remains for skinned casters.  
   *Alternatives:* CPU shadow only (Grill rejected); Hi-Z in shadow (extra maps, not in the grilled stories).

6. **Receiver id is MeshRenderer, 14 bits**  
   Pack 14-bit slot + unlit + two-sided in `R16_UINT` (16384 static GPU-driven MeshRenderers). CPU skinned/transparent keep the 256 UBO slot cap. Lighting still links per MeshRenderer.  
   *Alternatives:* keep 8-bit slot (Sponza primitives exceed 256); unique id per Meshlet (breaks linking); `R32_UINT` (unnecessary if 14 bits suffice).

7. **Sponza scene is Test-project content**  
   `Assets/Scenes/sponza.scene.asset`: Main Camera, Directional Light, GltfSceneImporter under a root using existing `Sponza.mesh.yaml`. Engine tests use a tiny synthetic mesh, not Crytek.  
   *Alternatives:* copy Sponza into the engine repo (Grill rejected).

8. **Culling compute stays on PRIMARY / graphics queue**  
   Same pattern as pick broad-phase. Mesh raster still SECONDARY inside the existing render pass. No Job-system GPU work.  
   *Alternatives:* render worker recording (ADR 0059 v1 no); compute queue (ADR 0060 v1 no).

## Risks / Trade-offs

- [False Hi-Z occlusion on camera teleport] → Late pass restores; first frame after a large cut can skip Hi-Z when view jumps past a threshold.
- [Mesh shader subgroup vs Slang] → Layout FATAL + a host test that the compute path’s indirect commands match CPU frustum/cone for a fixture mesh.
- [Version 3 recook of every static mesh] → Cook-if-stale; document delete `.blunder/cooked` if header mismatch lingers.
- [14-bit receiver still overflow] → Truncate with log like today’s 256, not FATAL; Sponza fits.
- [Alpha-clip Meshlets and cone cull] → Cone from MeshOptimizer; clip stays in fragment/mesh shader so holes still punch.

## Migration Plan

1. FetchContent MeshOptimizer; Meshlet cook + tests; recook static meshes.
2. Instance buffer + compute cull + VS/FS indirect into Forward; then Deferred G-buffer; then mesh shaders; then Hi-Z early/late; then shadow.
3. Author Test project Sponza scene last (needs the path live).
4. Rollback: revert; leftover version 3 Finals still load vertices; GPU-driven absent → CPU list.

## Open Questions

None.
