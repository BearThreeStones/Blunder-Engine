# Frame graph resolve attaches External RHI at Setup

CPU import stored only a Frame graph resource desc. We decided External import attaches a non-owning `rhi::IGpuTexture*` or `rhi::IGpuBuffer*` (not `VkImage`, not `void*`). Setup may read that pointer back; compile still does not allocate GPU memory. Null import fails compile as `InvalidImport`. Tests use dummy stand-ins without a Vulkan device. Domain: [CONTEXT.md — Frame graph resolve](../../CONTEXT.md). CPU DAG: [ADR 0061](0061-frame-graph-cpu-setup.md).

**Status:** accepted

## Considered Options

- **Store `VkImage` / `VkBuffer` on import** — rejected. Graph would include Vulkan; tests could not stay device-free. Viewport `OffscreenRenderTarget` stays a later wire-viewport slice.
- **Opaque `void*`** — rejected. A Texture handle could attach an arbitrary pointer.
- **Separate `bind()` after import** — rejected. Two attach paths; import is when the External already exists.
- **Keep a shape argument on import** — rejected. Pointer type is the shape. `createTransient` still takes shape because it has no object.
- **Throw on null import** — rejected. Compile is the error channel, same as illegal desc.
- **Fold null import into `InvalidDesc`** — rejected. Desc fields may be legal. New reason `InvalidImport`. Unused null still fails. Priority: `InvalidPass`, then `InvalidDesc`, then `InvalidImport`, then `DanglingAccess`, then `NoSink`, then `Cycle`.
- **Resolve only inside `execute()` callbacks** — rejected. Callbacks stay `void()` this slice. Lookup is Setup state, like `resourceDesc`. DCE does not erase. Miss cases return null and do not throw.
- **Extend ADR 0061 instead of 0063** — rejected. 0061 is the CPU DAG with no RHI types. ADR 0062 is the Deferred Render Path.
