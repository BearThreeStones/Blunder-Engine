## 1. Device table

- [x] 1.1 Require descriptor indexing at `VulkanContext` device pick (runtime arrays, non-uniform sampled-image/sampler, update-after-bind / partially-bound on the array bindings). Missing support fails device init; no per-draw color-texture fallback.
- [x] 1.2 Own one Bindless texture table on `VulkanContext`: set-1 layout with 1024 sampled images + 1024 samplers, UPDATE_AFTER_BIND pool, one set. Slot 0 is the fallback texture/sampler.
- [x] 1.3 Register/unregister GPU color textures with stable indices (first free slot ≥ 1; no compact on unload). Full table returns 0 and logs; does not FATAL.

## 2. Shaders and record path

- [x] 2.1 Rewrite `pbr.slang` / `pbr_skinned.slang`: color textures sample set-1 arrays by UBO `uint` indices; keep UBO, shadow comparison, and bone palette on set 0. Overlay / SSAO / pick / shadow-depth / grid / gizmo shaders stay as they are.
- [x] 2.2 `ForwardRenderPath` (and mesh-preview record path): bind the table set once per mesh pass; per draw write UBO indices + shadow; stop per-draw color-texture descriptor writes. Exact-match FATAL expected bindings match the new extract (compact set 0 plus the two array bindings).
- [x] 2.3 Update `k_pbr_descriptor_binding_count` / skinned counts and `fillSequentialExpectedBindings` call sites so init FATAL still matches record-path writes.

## 3. Tests

- [x] 3.1 Host test: stable index for the same texture, new texture gets another index, unload frees that slot only, overflow returns 0. Isolate `BLUNDER_GPU_CACHE_DIR` if the test compiles shaders.
- [x] 3.2 Update `shader_resource_layout_test` expected binding sets for `pbr.slang` / `pbr_skinned.slang`. Existing shadow/grid extracts still pass.

## 4. Docs

- [x] 4.1 Keep `CONTEXT.md` Bindless texture table / Forward mesh draw cap aligned. ADR 0056 stays the indexing + shadow-out-of-table record.
