## Why

CPU Frame graph import still records only a desc. Handle→RHI needs a non-owning device object on each External so later Execute can bind real images without treating the handle as a Vulkan identity. This slice attaches that pointer at import and lets Setup read it back.

## What Changes

- **BREAKING:** `importExternal` becomes two overloads. Shape follows the pointer: `importExternal(desc, IGpuTexture*, debug_name)` and `importExternal(desc, IGpuBuffer*, debug_name)`. `createTransient` still takes shape plus desc; it does not take an RHI pointer.
- Import stores a non-owning pointer. The graph does not own, allocate, or free GPU memory. Dummy `IGpuTexture` / `IGpuBuffer` stand-ins are enough for tests; no Vulkan device.
- `FrameGraph::resolvedTexture(handle)` and `resolvedBuffer(handle)` are Setup queries (no compile). DCE does not erase the pointer. Never-created, Transient (this slice), shape mismatch, and null import return `nullptr` and do not throw.
- Setup accepts a null import pointer and does not throw. Compile fails with `InvalidImport` even if DCE would drop that External. Priority: `InvalidPass`, then `InvalidDesc`, then `InvalidImport`, then `DanglingAccess`, then `NoSink`, then `Cycle`. After `InvalidImport`, `execute()` is `NotCompiled`.
- Graph header forward-declares `rhi::IGpuTexture` and `rhi::IGpuBuffer`. No `vulkan.h`. Callbacks stay `void()`.
- New [ADR 0063](../../../docs/adr/0063-frame-graph-resolve.md). [ADR 0061](../../../docs/adr/0061-frame-graph-cpu-setup.md) points here for External attach. ADR 0062 remains Deferred Render Path.
- First-party `frame_graph_test` covers the five User stories. Still no Transient GPU allocation, barriers, command recording, or `ForwardRenderPath` replacement.

## User stories

1. I import an External Texture with a legal desc and a dummy `IGpuTexture*`: before compile, `resolvedTexture` is that pointer; the resource stays External; no Vulkan device.
2. I import an External Buffer (size 256) with a dummy `IGpuBuffer*`: `resolvedBuffer` is that pointer; `resourceDesc` size is still 256.
3. I import with a null pointer, including an External no Pass uses: compile fails with `InvalidImport`, does not throw, live order is empty; `resolved*` is `nullptr`; `execute()` is `NotCompiled`.
4. The same graph has a never-added Pass handle, an illegal desc, a null import, and a dangling access: compile reports only `InvalidPass`. With the illegal Pass removed, illegal desc plus null import plus dangling reports `InvalidDesc`. With a legal desc, null import plus dangling reports `InvalidImport`.
5. Transient, a Texture handle asked as `resolvedBuffer`, and a never-created handle all return `nullptr` without throwing. A DCE'd External still returns the pointer that was imported. Clear→Forward Sink plus DCE still compile and execute when every External used on the live path has a dummy non-null pointer.

## Capabilities

### New Capabilities

- *(none)*

### Modified Capabilities

- `frame-graph`: import attaches a non-owning RHI pointer; Setup `resolvedTexture` / `resolvedBuffer`; compile `InvalidImport` with the locked priority; still no GPU allocation or command recording.

## Impact

- **Engine:** `engine/src/runtime/function/render/frame_graph/` import signatures, stored pointers, resolve getters, compile `InvalidImport`. Forward-declare RHI types only. Not a Context System.
- **Tests:** extend `engine/src/tests/frame_graph_test.cpp` with dummy RHI stand-ins. Existing import call sites take a dummy pointer. Link stays `engine_runtime` only.
- **Docs:** CONTEXT Frame graph resolve / External / compile (Grill); ADR 0063; ADR 0061 pointer.
- **Non-goals:** Transient GPU allocation; memory aliasing; barriers; changing `setExecute` to pass command buffers; replacing ForwardRenderPath; `VkImage` on the graph; JSON; Blackboard; Frame arena; Job System scheduling compile.
