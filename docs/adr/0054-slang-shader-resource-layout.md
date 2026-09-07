# Slang extracts Shader resource layout

VulkanPipeline builds Pipeline layout from the Shader resource layout Slang reports on the linked graphics program, at the same in-process compile that emits SPIR-V. Rejected: hand-authored C++ binding flags as the source of truth (they drifted from `[[vk::binding]]`), SPIR-V-Reflect after compile (second parser, extra library, unused on a future DXIL path), and a third layout YAML beside the Engine shader.
