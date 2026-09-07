## Why

Mesh pipelines still author `VkDescriptorSetLayout` from C++ feature flags while Engine shaders independently declare `[[vk::binding]]`. Those two tables drift, and every new texture slot is a dual edit. This is the first of three grilled render fills (layout → Engine GPU cache → Bindless texture table): make the compiled Engine shader the source of the Pipeline layout.

## What Changes

- **VulkanPipeline** family (forward opaque/transparent/shadow/skinned, plus grid/gizmo that already use this class) create **Pipeline layout** from **Shader resource layout** extracted via Slang at process init — same compile that emits SPIR-V. Not persisted; not Cook; not an Asset.
- Remove layout-authoring flags on `GraphicsPipelineDesc` / `VulkanPipelineCreateInfo` (`enable_texture_sampling`, `enable_shadow_sampling`, `enable_pbr_texture_sampling`, `enable_bone_palette` as layout switches). Vertex input, blend, depth, and topology stay CPU pipeline state.
- Record path still writes numeric bindings. Init **FATAL** unless the extracted binding set equals the set that record path writes. No name-to-binding bind API. No `PARTIALLY_BOUND`.
- Transparent mesh pipelines keep sharing the opaque Pipeline layout when they use the same Engine shader.
- Outline, SSAO, pick, and other one-off `vkCreateDescriptorSetLayout` paths stay hand-written.

## User stories

1. Open the Test Project; the editor viewport shows the usual PBR scene (opaque, transparent if present, shadows if enabled, skinned if present) with textures in the right slots.
2. Open a Mesh Asset that has material textures in Content Browser; Mesh Preview still auto-frames and shows the correct surface.
3. Ground grid, Transform gizmo, and Navigate gizmo still draw in the viewport (they use the same VulkanPipeline class).
4. Deliberately make an Engine shader’s binding set disagree with what the draw path writes: the process FATAL before a viewport appears, and does not run with a wrong layout. (Headless / a local broken shader is enough.)

## Capabilities

### New Capabilities

- `shader-resource-layout`: Derive Shader resource layout from a compiled Engine shader (Slang), build Pipeline layout from it for the VulkanPipeline family, and refuse to start when that binding set is not exactly the record path’s writes.

### Modified Capabilities

- *(none — overlay/pick/SSAO product behavior is unchanged; those passes keep hand-written layouts.)*

## Impact

- **Engine:** `SlangCompiler` layout extract on the linked VS+FS program; `VulkanPipeline` / `GraphicsPipelineDesc`; `ForwardRenderPath` (declare written bindings for the FATAL check); mesh preview backend that copies the same pipeline descs; tests under `engine/src/tests/`.
- **Docs:** `CONTEXT.md` Rendering terms from Grill; [ADR 0054](../../../docs/adr/0054-slang-shader-resource-layout.md).
- **Non-goals:** Shader bytecode cache, Pipeline cache, Bindless texture table, SPIR-V-Reflect, Engine shaders as Assets, D3D12 pipeline objects, outline/SSAO/pick layout generation, raising the Forward mesh draw cap.
