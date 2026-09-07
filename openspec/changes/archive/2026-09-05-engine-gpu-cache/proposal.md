## Why

Every editor and Player start still compiles Engine shaders with Slang and creates pipeline objects with `VkPipelineCache` null. Layout extract already runs in-process (change `shader-resource-layout`); the slow work is thrown away. This is the second of three grilled render fills (layout → Engine GPU cache → Bindless texture table): persist Shader bytecode cache and Pipeline cache so a later start on the same machine skips that work.

## What Changes

- Persist **Shader bytecode cache**: SPIR-V from an Engine shader compile, stored with the Shader resource layout from that same compile. A later process start that still matches the cache key skips Slang. Do not re-parse SPIR-V with SPIR-V-Reflect.
- Persist **Pipeline cache**: the Vulkan driver `VkPipelineCache` blob for this device, so `vkCreateGraphicsPipelines` can reuse driver work. This does not replace Slang when bytecode is stale.
- Store both under **Engine GPU cache**: a user-level directory outside any Project. Editor and Player on that machine share it. Not `.blunder/`, not Cooked cache, not an Asset.
- Miss / rebuild (never FATAL for cache itself): Engine shader source change → bytecode miss; `pipelineCacheUUID` change → Pipeline cache miss (bytecode may still hit); engine/Slang compile-identity change → both miss; corrupt blob → discard and rebuild. A Shader resource layout that does not match the record path still FATALs at pipeline init (not a cache miss).
- Unchanged: numeric record-path binds, exact-match FATAL, outline/SSAO/pick hand-written layouts, no Bindless texture table, no disk cache inside `.blunder/cooked/`.

## User stories

1. After emptying the user-level Engine GPU cache, the first editor open draws the viewport correctly; a second open of the same editor on the same GPU skips full Slang compile for unchanged Engine shaders, and the picture still matches.
2. After editing `pbr.slang` and starting again, that shader’s bytecode cache misses and the viewport is still correct.
3. After corrupting a cache file and starting again, the editor still starts and the viewport is still correct (treat as miss, rebuild, do not FATAL).
4. Enter Play from the editor: Player is another process, still uses the same user-level cache directory, and Play looks correct.

## Capabilities

### New Capabilities

- `engine-gpu-cache`: User-level Shader bytecode cache (SPIR-V + Shader resource layout) and Pipeline cache for Engine shaders, shared by Editor and Player, with miss/rebuild rules that never treat a bad blob as process-start failure.

### Modified Capabilities

- *(none — `shader-resource-layout` stays an unarchived change; this does not change its FATAL contract. Play still spawns a separate Player process.)*

## Impact

- **Engine:** `SlangCompiler::compileGraphicsProgram` (and Engine-shader `compileShader`) read/write bytecode; `VulkanContext` owns `VkPipelineCache` load/save; `VulkanPipeline::createGraphicsPipeline` passes that cache; path helper beside Project List’s user-level home.
- **Hosts:** Editor and Player resolve the same default Engine GPU cache directory.
- **Docs:** `CONTEXT.md` Rendering terms (already grilled); [ADR 0055](../../../docs/adr/0055-engine-gpu-cache-user-level.md).
- **Non-goals:** Bindless texture table, SPIR-V-Reflect, Engine shaders as Assets, Cook, Project-local Engine shader recompile as the hit path, raising the Forward mesh draw cap.
