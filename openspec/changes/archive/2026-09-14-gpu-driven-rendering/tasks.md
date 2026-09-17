## 1. Meshlet cook

- [x] 1.1 Add MeshOptimizer via CMake FetchContent (not under `engine/3rdparty/`). Link it into mesh Cook only.
- [x] 1.2 Bump mesh Final to version 3: append Meshlet vertices/triangles, sphere, and cone after the existing vertex/index/skin payload. Version ≤2 and Fast Path stay valid without Meshlets.
- [x] 1.3 Generate Meshlets at Cook for static meshes (`meshopt_buildMeshlets` 64/124 + `meshopt_computeMeshletBounds`). Skinned Cook MAY omit Meshlets.
- [x] 1.4 Add `meshlet_cook_test` (or equivalent stem): a tiny static fixture cooks to Meshlets with sphere/cone; a skinned fixture MAY have none. Build and run that test (`build/vs2026-debug`, Debug).

## 2. Instance buffer and CPU/GPU split

- [x] 2.1 Upload a per-frame GPU instance buffer for static opaque/alpha-clip MeshRenderers that have Meshlets (world matrix, bindless indices, meshlet range, MeshRenderer receiver id). Meshes without Meshlets stay on the CPU `ForwardOpaqueDraw` list.
- [x] 2.2 Keep skinned, blend-transparent, pick, outline, Mesh Preview, Camera Preview, and Scene Thumbnail on the CPU list (cap 256). GPU-driven static SHALL NOT use that cap.

## 3. Compute cull and VS/FS indirect (Forward)

- [x] 3.1 Graphics-queue compute: frustum + cone cull writing indirect commands. Exact-match FATAL on the new compute layout. No dedicated compute queue.
- [x] 3.2 Record GPU-driven static into the Forward opaque pass via `vkCmdDrawIndexedIndirectCount` (or equivalent) using existing PBR fragment + Bindless. CPU list still `vkCmdDrawIndexed`.
- [x] 3.3 Add `meshlet_cull_test` (CPU mirror of cone + frustum accept/reject on a fixture). Build and run it (`build/vs2026-debug`, Debug).
- [x] 3.4 Update `shader_resource_layout_test` expected bindings for new compute/graphics shaders. Build and run that test.

## 4. Mesh shaders

- [x] 4.1 Query `VK_EXT_mesh_shader` at device create; missing extension SHALL NOT fail init.
- [x] 4.2 When present, task + mesh shaders consume early/late Meshlet lists and output the same fragment inputs as the VS path. When absent, keep compute + VS/FS. Exact-match FATAL on task/mesh layouts.

## 5. Hi-Z early/late

- [x] 5.1 Path-owned depth pyramid from the previous offscreen depth (FIF like G-buffer). Not a Frame graph Transient. First frame treats Hi-Z as empty.
- [x] 5.2 Early pass: frustum + cone + Hi-Z; draw survivors; late pass retests Hi-Z rejects and draws. No depth prepass. No visibility buffer.

## 6. Deferred and shadows

- [x] 6.1 GPU-driven static writes the G-buffer (alpha clip in geometry). Widen receiver id to 14-bit MeshRenderer slot + unlit + two-sided in `R16_UINT`. Meshlets inherit the MeshRenderer id. Update deferred lighting to that packing.
- [x] 6.2 Directional shadow pass: GPU-driven static via frustum/cone (no Hi-Z). Skinned casters stay on the CPU shadow list.

## 7. Sponza scene and docs

- [x] 7.1 In the Test project only, add `Assets/Scenes/sponza.scene.asset` (Main Camera, Directional Light, existing Sponza mesh via the glTF importer). Do not copy Crytek files into Blunder-Engine.
- [x] 7.2 Recook Test project static meshes so Sponza Finals contain Meshlets.
- [x] 7.3 Update `docs/agents/render-pipeline.md` for GPU-driven static, Hi-Z, mesh-shader fallback, and the CPU-list remainder. Keep CONTEXT / ADR 0070 aligned.
- [x] 7.4 Build `engine_editor` (`build/vs2026-debug`, Debug).
