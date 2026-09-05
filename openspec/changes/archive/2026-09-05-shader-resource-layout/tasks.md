## 1. Extract Shader resource layout from Slang

- [x] 1.1 Add a Shader resource layout value type (set, binding, descriptor kind) and extract it from a linked Slang graphics program (VS+FS), not from a single entry point.
- [x] 1.2 Refactor `SlangCompiler` / `VulkanShader::loadFromSlang` so vertex and fragment share one session/link; SPIR-V for both stages and layout come from that link.
- [x] 1.3 Binding-set equality helper: extracted set vs record-path expected set; mismatch is a distinct failure (FATAL at pipeline init).

## 2. VulkanPipeline uses extracted layout

- [x] 2.1 `VulkanPipeline::createDescriptorSetLayout` builds `VkDescriptorSetLayout` / `VkPipelineLayout` from the extracted layout. Same-shader `shared_descriptor_set_layout` still skips a second extract.
- [x] 2.2 Each shared-path pipeline declares the bindings its record path writes (opaque 0–10, skinned +11, shadow 0, skinned shadow + bone palette, grid/gizmo 0). Init FATAL on inequality.
- [x] 2.3 Remove layout-authoring flags from `GraphicsPipelineDesc` / `VulkanPipelineCreateInfo` and call sites (`RenderSystem`, mesh preview). Keep vertex input, blend, depth, topology, cull.

## 3. Tests

- [x] 3.1 `shader_resource_layout_test`: extract `pbr.slang`, `pbr_skinned.slang`, `shadow_depth.slang`, `grid.slang`; assert binding sets. Assert compare helper fails when sets differ.
- [x] 3.2 Wire the test in `engine/src/tests/CMakeLists.txt` (links `engine_runtime`; needs `slang.dll` on PATH like other render-adjacent tests).

## 4. Docs

- [x] 4.1 Keep `CONTEXT.md` Rendering terms aligned if code names differ. ADR 0054 stays the Slang-vs-Reflect record.
