## 1. Engine GPU cache store

- [x] 1.1 Path helper: default `%LOCALAPPDATA%/Blunder/gpu-cache/` (Windows) / XDG cache `Blunder/gpu-cache`; honor `BLUNDER_GPU_CACHE_DIR` when set to an absolute path. Editor and Player share the default.
- [x] 1.2 Bytecode blob: magic + format version + compile identity (Slang build tag + SPIR-V profile) + source SHA-256 + entry names; payload VS/FS SPIR-V + packed Shader resource layout. Atomic temp+rename. Integrity failure deletes the file and returns miss.
- [x] 1.3 First-party SHA-256 helper (no new third-party). Wire sources in `engine/src/runtime/function/render/CMakeLists.txt`.

## 2. Slang and Pipeline cache

- [x] 2.1 `compileGraphicsProgram` and Engine-shader `compileShader` try bytecode hit before Slang; on miss compile, extract layout (graphics program only), write cache. Log hit vs miss. Write/IO failure does not FATAL.
- [x] 2.2 `VulkanContext` loads/saves one `VkPipelineCache` keyed by `pipelineCacheUUID` + generation. Failed create → empty cache, delete bad blob. `VulkanPipeline::createGraphicsPipeline` passes that handle. Other `vkCreateGraphicsPipelines` on that device use it when they already hold `VulkanContext`.
- [x] 2.3 Cache hit still runs the existing Shader resource layout vs record-path compare; mismatch remains FATAL, not a miss.

## 3. Tests

- [x] 3.1 `engine_gpu_cache_test` with `BLUNDER_GPU_CACHE_DIR` temp dir: hit restores SPIR-V+layout and skips a second Slang compile; source-byte change misses; corrupt file misses and still succeeds; default path is identical for a simulated Editor/Player lookup. `WORKING_DIRECTORY` repo root; `blunder_copy_runtime_dlls`.
- [x] 3.2 Existing `shader_resource_layout_test` still passes (first compile may fill the override dir).

## 4. Docs

- [x] 4.1 Keep `CONTEXT.md` Engine GPU cache / Shader bytecode cache / Pipeline cache aligned. ADR 0055 stays the user-level vs Project `.blunder/` record.
