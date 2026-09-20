# Tasks

## 1. Device query and lighting RenderPass2

- [x] 1.1 Query `VK_KHR_fragment_shading_rate` plus `attachmentFragmentShadingRate` / `pipelineFragmentShadingRate` at device create (same optional pattern as `VK_EXT_mesh_shader`), and verify a missing extension logs and does **not** FATAL device init
- [x] 1.2 When enabled, create the lighting render pass with RenderPass2 and `VkFragmentShadingRateAttachmentInfoKHR`; keep `VK_SUBPASS_CONTENTS_SECONDARY_COMMAND_BUFFERS`; verify the no-extension path still uses today’s color-only lighting RP
- [x] 1.3 Choose shading-rate attachment texel size from `VkPhysicalDeviceFragmentShadingRatePropertiesKHR` (`{1,1}` if in range, else device min), and verify a unit test does not hardcode a size outside that range

## 2. Rate image and Sobel compute

- [x] 2.1 Add path-owned FIF `R8_UINT` rate images (one set per deferred offscreen, not Bindless, not Frame graph Transient), clear texel `0` (1×1), and verify resize/drop follows G-buffer `dropGpuTargets`
- [x] 2.2 Add graphics-queue Slang Sobel compute (Rec.709 luminance, 3×3, 16×16 / 18×18 halo, `G > 0.1` → 0 else `1 << 2 | 1`), dispatch after lighting RP and before LOAD / copy, and verify `shader_resource_layout_test` exact-match plus a CPU twin for luminance / threshold / encode
- [x] 2.3 In the lighting SECONDARY, `vkCmdSetFragmentShadingRateKHR` with pipeline 1×1 and attachment REPLACE; sample the previous FIF slot; leave `deferred_lighting.slang` unmodified
- [x] 2.4 Honor `BLUNDER_EDITOR_VRS=0` as force-off (no FSR attachment, no Sobel) on every deferred lighting pass, and verify the env does not become a product settings page

## 3. Deferred Player / Preview / Thumbnail

- [x] 3.1 Construct `DeferredRenderPath` in the Player and record G-buffer then Lighting on the Player viewport graph (not Forward Scene), and verify Placement Preview is untouched Forward
- [x] 3.2 Record Camera Preview as deferred into its dedicated offscreen after viewport `execute` (unshadowed, no Editor Overlays, no mask overlay), and verify it is not a viewport-graph Pass
- [x] 3.3 Record Mesh Preview Render and Scene Thumbnail / Capture as deferred (Studio lighting vs Light Components unchanged), and verify they no longer call Forward `renderFrameTo` for opaque lighting
- [x] 3.4 Run clustered froxel fill + clustered lighting on those deferred surfaces, and verify remaining Forward (`BLUNDER_EDITOR_DEFERRED=0` editor Viewport, Placement Preview) still uses the flat 8-light cap

## 4. Overlay, hosts, DogWalk contract (apply later; not this planning commit)

- [x] 4.1 Add a Viewport View-menu rate-mask overlay (Figure 9.4 two colours, default off, not persisted) next to Froxel Occupancy Heatmap, and verify Player / Preview / Thumbnail never draw it
- [x] 4.2 Skip requiring the extension on Headless / CLI / MCP / Linux Merge CI, and verify those hosts still start
- [x] 4.3 Human walk remains DogWalk `assets/Scenes/se-world.scene.asset` in windowed `engine_editor` and Play (not Test/Sponza, not collision QC `root.scene.asset`); checklist stays **Not run** until the human walks it
- [x] 4.4 Do not edit `cursor/scene-collision-bridge-8a89`, PR 24, or PR 36; verify this change’s diff has no files from those branches

## 5. Docs and validate

- [x] 5.1 Keep `CONTEXT.md` Variable rate shading / Deferred / Player terms aligned with implementation names; ADR 0074 stays the image-based lighting-attachment record; on apply update `docs/agents/render-pipeline.md`
- [x] 5.2 `openspec validate variable-rate-shading --strict` passes
