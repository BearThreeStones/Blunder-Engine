## 1. Import attach and resolve

- [x] 1.1 Forward-declare `rhi::IGpuTexture` and `rhi::IGpuBuffer` on `frame_graph.h`. No `vulkan.h`. Add `FrameGraphCompileReason::InvalidImport`.
- [x] 1.2 Replace `importExternal(shape, desc, name)` with `importExternal(desc, IGpuTexture*, name)` and `importExternal(desc, IGpuBuffer*, name)`. Store the non-owning pointer. `createTransient` still takes shape plus desc.
- [x] 1.3 Add `resolvedTexture(handle)` and `resolvedBuffer(handle)`: Setup state, no compile. DCE does not erase. Never-created, Transient, shape mismatch, and null import return `nullptr` and do not throw.
- [x] 1.4 Compile: null External pointer is `InvalidImport` even if unused. Priority: `InvalidPass`, then `InvalidDesc`, then `InvalidImport`, then `DanglingAccess`, then `NoSink`, then `Cycle`.

## 2. Tests

- [x] 2.1 Dummy `IGpuTexture` / `IGpuBuffer` stand-ins in `frame_graph_test` (no Vulkan device). Update every existing import to pass a dummy pointer. Cover the five User stories: Texture resolve before compile; Buffer resolve + size 256; unused null → `InvalidImport` + execute `NotCompiled`; `InvalidPass` beats `InvalidDesc` beats `InvalidImport` beats `DanglingAccess`; miss cases + DCE pointer + Clear→Forward.
- [x] 2.2 Build and run `frame_graph_test` (`build/vs2026-debug`, Debug).

## 3. ADR

- [x] 3.1 Keep [ADR 0063](../../../docs/adr/0063-frame-graph-resolve.md) and the [ADR 0061](../../../docs/adr/0061-frame-graph-cpu-setup.md) pointer aligned with the shipped import/resolve API.
