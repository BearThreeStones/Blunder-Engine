## Context

See proposal.md for why. Grill locked: both Shader bytecode cache and Pipeline cache; user-level Engine GPU cache (not Project `.blunder/`, not Cooked); miss table (source/version miss rebuilds; `pipelineCacheUUID` misses Pipeline cache only; corrupt = miss, never FATAL; binding mismatch stays FATAL); bytecode stores SPIR-V with the Shader resource layout from that compile so SPIR-V-Reflect stays out. ADR 0054 already covers Slang extract. ADR 0055 covers this store.

Today `SlangCompiler::compileGraphicsProgram` always reads `.slang` and links. `vkCreateGraphicsPipelines` passes `VK_NULL_HANDLE` for cache (`VulkanPipeline` and overlay/SSAO/pick). Project List already lives under `%APPDATA%/Blunder/` (Windows) / XDG config (elsewhere). There is no product `VERSION` file and no SHA helper in `engine/src`.

Change `shader-resource-layout` is the in-process extract + FATAL contract this cache must not weaken.

## Goals / Non-Goals

**Goals:**

- Hit path: skip Slang for an unchanged Engine shader; restore SPIR-V + Shader resource layout; still run the record-path FATAL compare.
- Hit path: create graphics pipelines with a loaded `VkPipelineCache`; save the blob at device teardown.
- Default directory shared by Editor and Player; tests can point at a temp dir.

**Non-Goals:**

- Bindless texture table.
- Session-level Slang module cache (in-process `ISession` reuse) as a substitute for disk cache.
- Caching DXIL / D3D12 pipeline objects.
- Making Engine shaders Assets or Cook output.

## Decisions

1. **Cache root is local user cache, not roaming config**  
   Windows: `%LOCALAPPDATA%/Blunder/gpu-cache/`. Else: `$XDG_CACHE_HOME/Blunder/gpu-cache` or `~/.cache/Blunder/gpu-cache`. Same vendor folder idea as Project List, different known-folder so SPIR-V and driver blobs do not roam with `%APPDATA%`.  
   *Alternatives:* `%APPDATA%/Blunder/gpu-cache/` next to `project_list.yaml` (blobs roam); Project `.blunder/` (Grill rejected); build output (clean wipes the hit path).

2. **Tests and CI override with `BLUNDER_GPU_CACHE_DIR`**  
   When set to an absolute directory, both hosts use that path instead of the default. Headless tests write/corrupt files there and never touch the author’s real cache.  
   *Alternatives:* only the default path (tests pollute user cache); a C++ setter only (Player spawn must still agree — env is the shared knob).

3. **Bytecode key = source bytes hash + entry names + compile identity**  
   Hash the file bytes (SHA-256 in a small first-party helper; no new third-party). Compile identity = blob format version + Slang build tag from the global session + SPIR-V profile string (`spirv_1_5` today). Filename under `bytecode/` includes that identity hash so two Slang builds do not overwrite the same slot. Payload: magic, format version, identity, VS SPIR-V, FS SPIR-V (empty for single-entry `compileShader`), packed `ShaderResourceLayout`. Write temp then rename.  
   *Alternatives:* mtime-only keys (false hits after copy); parse SPIR-V on hit (Grill rejected Reflect); one JSON sidecar (extra files, easy to desync).

4. **`compileGraphicsProgram` is the required client; `compileShader` writes bytecode too**  
   Overlay/SSAO/pick still use per-entry compile. Caching those SPIR-V files is the same store and cuts the other half of Engine shader Slang work. Layout field is empty for single-entry results.  
   *Alternatives:* graphics-program only (overlay still full Slang every boot).

5. **One `VkPipelineCache` per `VulkanContext`**  
   After physical device selection, load `pipelines/<uuid-hex>_g<generation>_<identity-hex>.bin` keyed by `pipelineCacheUUID`, the same generation constant as bytecode, and Slang compile identity (build tag + SPIR-V profile, hashed into the filename). `vkCreatePipelineCache` with that data; on failure create empty. `VulkanPipeline::createGraphicsPipeline` passes the handle. Other `vkCreateGraphicsPipelines` on that device use the same handle when they already have `VulkanContext`. Save `vkGetPipelineCacheData` on context shutdown.  
   *Alternatives:* per-pipeline files (driver cache is one blob); cache=NULL still (Grill rejected “only bytecode”); UUID+generation only (would keep the Pipeline cache across a Slang identity change).

6. **Corrupt / IO failure is miss, never FATAL**  
   Bad magic, truncated payload, hash mismatch, `vkCreatePipelineCache` failure, or failed write: log, delete the bad file if present, compile / create empty cache. Disk-full on write does not abort start.  
   *Alternatives:* FATAL on corrupt (Grill rejected); ignore write errors silently with no log.

7. **Binding mismatch stays FATAL after a hit**  
   Cache restore feeds the same compare `VulkanPipeline::initialize` already runs. Do not treat mismatch as “stale cache, recompile” — that would hide a record-path contract bug.  
   *Alternatives:* auto-recompile on mismatch (Grill rejected).

## Risks / Trade-offs

- [First boot still pays full Slang + PSO cost] → Expected; stories measure second start and miss/rebuild, not first-start parity with a warm cache.
- [Source hash ignores `#include` if Engine shaders later include files] → Today they are single files. If includes appear, extend the hash to included bytes or FATAL that the cache key is incomplete — do not silently hit.
- [Driver rejects a Pipeline cache blob after a quiet driver update with same UUID] → Treat `vkCreatePipelineCache` / first `vkCreateGraphicsPipelines` failure as miss: drop the blob, retry with empty cache once; do not abort start.
- [Two processes write the same bytecode file] → Atomic rename; last writer wins. Same source produces the same payload.
- [LOCALAPPDATA vs Grill “sibling to Project List”] → Same user-level / not-Project rule; cache vs config known-folders. Revisit only if authors need the blobs next to `project_list.yaml`.

## Migration Plan

1. Ship the store + keys; first boot fills the directory; no content migration.
2. Rollback: revert the change; optional leftover files under `Blunder/gpu-cache/` are unused and safe to delete.

## Open Questions

None.
