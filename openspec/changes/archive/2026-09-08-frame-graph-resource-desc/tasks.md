## 1. Desc types and GraphBuilder

- [x] 1.1 Add `FrameGraphFormat::{Undefined, R8G8B8A8_UNORM, D32_SFLOAT}` and `FrameGraphResourceDesc` (format, width, height, sample count, mip count, size). No RHI / Vulkan includes. Extent is `uint32` width/height, not `rhi::Extent2D`.
- [x] 1.2 Change GraphBuilder `createTransient` / `importExternal` to take the required desc (shape stays a separate argument). Store the desc on the resource. `resourceDesc(handle)` returns it without compile; never-created handle returns a default-constructed desc.
- [x] 1.3 Add `FrameGraphCompileReason::InvalidDesc`. Compile checks every created/imported desc (illegal Texture: `Undefined` / width or height 0 / samples or mips 0; illegal Buffer: size 0). Unused illegal desc still fails. Reason priority: `InvalidPass`, then `InvalidDesc`, then `DanglingAccess`, then `NoSink`, then `Cycle`.

## 2. Tests

- [x] 2.1 Add a legal-desc helper in `frame_graph_test` and pass it on every existing create/import. Cover the five User stories: Transient Texture desc before compile; imported External viewport-color desc; Buffer size 256; unused illegal desc → `InvalidDesc`; `InvalidPass` beats `InvalidDesc` beats `DanglingAccess`; execute after `InvalidDesc` is `NotCompiled`. Clear→Forward Sink and DCE still pass with legal desc.
- [x] 2.2 Build and run `frame_graph_test` (`build/vs2026-debug`, Debug).

## 3. ADR

- [x] 3.1 Extend [ADR 0061](../../../docs/adr/0061-frame-graph-cpu-setup.md): CPU Frame graph resource desc on create/import; `FrameGraphFormat`; `InvalidDesc` vs `DanglingAccess`; `resourceDesc` without compile. Do not add ADR 0062.
