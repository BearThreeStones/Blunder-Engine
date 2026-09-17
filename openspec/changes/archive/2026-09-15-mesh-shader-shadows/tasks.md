## 1. Prerequisites (Windows GPU-driven tree)

- [x] 1.1 Confirm cooked GPU-driven meshlet buffers (center, radius, cone, vertex/index blobs, opaque vs alpha flags) exist on the apply tree and record their layout against `design.md` Risks. Verify by dumping one cooked mesh's meshlet count > 0. If missing, stop apply — do not recook and do not amend archived `gpu-driven-rendering`.
- [x] 1.2 Confirm the deferred GBuffer (or equivalent scene-depth target) used by Viewport/Player can reconstruct world/view depth for VSM page mark. Verify by reading one frame's depth attachment format/size. Do not invent a new GBuffer.
- [x] 1.3 Confirm `clustered-deferred-local-lights` is not modified: no edits under that change folder, no `froxelLightIndices` reads in shadow page-mark. Verify `git diff` excludes that path.

## 2. RHI mesh shaders and fallback

- [x] 2.1 Enable `VK_EXT_mesh_shader` (and query `shaderOutputLayer`) at device init when present; leave descriptor-indexing as-is. Verify with a startup log or capability dump that meshShader is true on a supporting device and false does not FATAL.
- [x] 2.2 Add mesh/task depth-only pipeline (Slang → SPIR-V, empty/no FS, `SetMeshOutputsEXT` / `EmitMeshTasksEXT`, max 64 verts / 124 prims). Verify pipeline creation succeeds on a mesh-shader device.
- [x] 2.3 Keep VS/FS `shadow_depth.slang` (and skinned VS path) as the fallback when mesh shaders are missing. Verify a mesh-shader-less device still creates the classic shadow pipeline and does not create the mesh pipeline.

## 3. Shared meshlet caster recording

- [x] 3.1 Build per-view opaque meshlet caster lists from cooked buffers, skipping alpha-mask, transparent, skinned, and foliage. Verify a mixed scene records only opaque meshlets (debug counter or unit-style scan of the list).
- [x] 3.2 Honor Light linking: empty receiver list → all opaque meshlet MeshRenderers; non-empty → only those entities. Verify with a two-mesh scene that a linked light's caster list excludes the unlisted mesh.
- [x] 3.3 Wire the same recording into Viewport and Player; force Camera Preview `shadows_enabled = false` (ADR 0018); do not attach this pass to Mesh Preview / Placement Preview / Scene Thumbnail. Verify Camera Preview render does not begin a shadow fill.

## 4. Directional VSM clipmap

- [x] 4.1 Allocate directional clipmap resources: 128² pages, page table for virtual 16k, physical pool default 512 `D32` pages, FirstLevel 6, LastLevel 10, camera-centered, radius `2^(level+1)`. Verify buffer/image sizes match those constants (debug print or test).
- [x] 4.2 Implement GBuffer-depth pixel page mark into that clipmap. Verify a receiver in front of the camera marks at least one page and that clustered froxel lists are not read.
- [x] 4.3 Fill marked physical pages with the mesh-shader depth-only raster (task frustum/page cull + mesh emit). Verify a known occluder writes non-clear depth into a marked page (GPU capture or readback of one page).
- [x] 4.4 Sample via page-table translate + 2×2 `SampleCmp` PCF on the physical page; unmarked/overflow pages shade unshadowed; log pool overflow. Verify an occluded receiver darkens and a pixel whose page was never marked stays unshadowed from that miss.
- [x] 4.5 Do not allocate CSM cascade atlases or VSM pages for Point/Spot. Verify only the first shadowing Directional fills the clipmap (two-directional scene) and local lights' shadow resources are cube/2D only.
- [x] 4.6 Fallback device: keep the existing 1024² directional ortho VS fill (no VS-implemented VSM). Verify mesh-shader-less path still shows classic directional shadows.

## 5. Point cubemaps

- [x] 5.1 Allocate a 6-face cubemap (or 6-layer array) per shadowing Point Light, up to 8 per view (stable EntityId). Verify the 9th shadowing Point logs a drop and still illuminates.
- [x] 5.2 Fill faces with layered mesh dispatch when `shaderOutputLayer` is available; otherwise six mesh (or VS fallback) face passes. Verify all six faces receive depth for a meshlet at the light and that no Nanite visbuffer emit pass exists.
- [x] 5.3 Sample with existing 2×2 hardware PCF (not SMRT, not Vogel). Verify an occluded receiver inside range is shadowed and a receiver beyond range is not.

## 6. Spot 2D maps

- [x] 6.1 Allocate one 2D perspective depth map per shadowing Spot Light (outer cone FOV, far = Light range), up to 8 per view. Verify the 9th shadowing Spot logs a drop and still illuminates, and that no cubemap/VSM is created for a Spot.
- [x] 6.2 Fill with the same meshlet depth-only path (task cone/frustum cull). Verify a meshlet inside the cone writes depth and a meshlet outside does not.
- [x] 6.3 Sample with existing 2×2 PCF. Verify an occluded in-cone receiver is shadowed.

## 7. Product wiring and glossary

- [x] 7.1 Expand caster pick so Point/Spot with contribution including shadows cast; Area does not; Shadows-only Point/Spot occlude without adding radiance. Verify the four `scene-light-component` delta scenarios in a live or Headless scene.
- [x] 7.2 Remove `BLUNDER_EDITOR_SHADOWS` as the Viewport product on-gate so Viewport shows shadows whenever casters exist (Player already does). Verify Viewport shadows appear without that env var.
- [x] 7.3 Keep shadow resources off the Bindless color table (page table, physical pages, cubes, spot maps, fallback map). Verify those images are not Bindless table entries (dump or existing bindless test extended).
- [x] 7.4 Update `CONTEXT.md` **Light shadows** and `docs/agents/render-pipeline.md` shadow notes to match this slice. Verify the glossary no longer says Point/Spot do not cast.

## 8. Acceptance

- [x] 8.1 Run `openspec validate mesh-shader-shadows --strict` and the project build/test commands in `docs/agents/testing.md` for touched areas; confirm a clean pass before requesting Human acceptance.
- [x] 8.2 Walk `manual-checklist.md` on a Windows build that has GPU-driven meshlets (all 5 User stories) and record actual results. Do not sign Human acceptance.
