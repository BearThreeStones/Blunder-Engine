## Context

See proposal.md for why. Grill locked: Shader resource layout (not the Reflection kernel); Pipeline layout from that layout; VulkanPipeline family only; generate at in-process Slang compile; no persist; record path still uses numeric bindings; exact-match FATAL; extract via Slang, not SPIR-V-Reflect. ADR 0054.

Today `VulkanPipeline::createDescriptorSetLayout()` builds bindings from `enable_texture_sampling` / `enable_shadow_sampling` / `enable_pbr_texture_sampling` / `enable_bone_palette`. `SlangCompiler::compileShader` creates a new session per entry point and returns SPIR-V only. Transparent pipelines pass `shared_descriptor_set_layout` from the opaque pipeline (same `pbr.slang` / `pbr_skinned.slang`). Outline, SSAO, pick, pick-compute create layouts themselves.

D3D12 graphics pipelines remain stubs. Engine shaders live under `engine/shaders/`.

## Goals / Non-Goals

**Goals:**

- Extract a binding list from the linked VS+FS Slang program and create `VkDescriptorSetLayout` / `VkPipelineLayout` from it.
- Each shared-path pipeline declares the binding numbers its record path writes; init FATAL on set inequality.
- Drop layout-authoring flags from `GraphicsPipelineDesc`. Raster/vertex-input flags stay.
- Keep transparent sharing when the shader path is the same.

**Non-Goals:**

- Disk Shader bytecode cache / Pipeline cache (next change).
- Bindless texture table (change after cache).
- SPIR-V-Reflect, ParameterBlock rewrite, push constants, vertex-input reflection.
- Generating layouts for outline / SSAO / pick.

## Decisions

1. **Slang `IComponentType` layout on the linked graphics program**  
   Compile vertex and fragment entry points into one linked program (or compose the two components then `link`), then read `getLayout()` / parameter binding ranges. Do not compile each stage in isolation for layout, and do not parse SPIR-V.  
   *Alternatives:* SPIR-V-Reflect (rejected, ADR 0054); per-stage layouts merged by hand (easy to miss a fragment-only binding).

2. **Expected bindings are a record-path set, not layout-authoring flags**  
   Forward opaque writes 0–10; skinned adds 11; shadow writes 0; skinned shadow writes 0 and the bone-palette binding; grid/gizmo write 0. Store that set next to the pipeline that records, compare to the extracted set, FATAL on mismatch.  
   *Alternatives:* name-to-binding bind API (next-but-one bindless would throw it away); validation-layer-only (Release can miss it).

3. **`shared_descriptor_set_layout` stays for same-shader raster variants**  
   Transparent copies opaque’s layout handle when `shader_path` is identical. Do not extract twice. If shader paths differ, extract independently (skinned vs unskinned).  
   *Alternatives:* always extract (duplicate identical layouts); require every pipeline to own a layout (wastes handles, two FATAL checks for the same shader).

4. **Keep `[[vk::binding]]` numbers; CPU still `vkUpdateDescriptorSets` by those numbers**  
   Layout generation does not change draw. Flags that only existed to build the layout go away; texture/shadow/bone still happen because the shader and the write functions say so.  
   *Alternatives:* bind-by-name table this slice (Grill rejected).

5. **Grid/gizmo ride the same `VulkanPipeline` extract path**  
   Their Engine shaders already declare binding 0. Changing `createDescriptorSetLayout` once covers them. Outline/SSAO/pick stay out.  
   *Alternatives:* mesh-only branch inside `VulkanPipeline` (two layout builders).

6. **Tests compile Engine shaders through `SlangCompiler` without requiring a window**  
   Headless: extract `pbr.slang` / `pbr_skinned.slang` / `shadow_depth.slang` / `grid.slang` and assert binding sets. A second case: extracted set ≠ expected → the compare helper fails (the FATAL path). Do not require `engine_editor` for the mismatch story.  
   *Alternatives:* editor-only smoke as the only proof (weak for story 4).

## Risks / Trade-offs

- [Slang layout vs explicit `[[vk::binding]]` disagreement] → FATAL at init; treat as a compiler bug, do not silently remap.
- [Unused shader bindings still appear in layout] → Exact match means the record path must write them (today it already fills fallback textures) or the shader must not declare them.
- [Two compiles today, one layout needed] → Refactor `loadFromSlang` / `SlangCompiler` so VS+FS share one session/link; avoid doubling Slang work.
- [D3D12 stub] → Vulkan-only extract→`Vk*` this slice; keep RHI desc free of Vulkan handles except the existing `shared_descriptor_set_layout` uint64.
- [Grid/gizmo shaders with only a UBO] → Expected set `{0}`; if extract returns extra bindings, FATAL surfaces immediately.

## Migration Plan

1. Add extract + compare helpers; wire `VulkanPipeline`; delete layout flags; keep raster flags.
2. No content migration. First boot still compiles from `engine/shaders/` source.
3. Rollback: revert the change; no on-disk layout to clean up.

## Open Questions

None.
