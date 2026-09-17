## Why

CPU Frame graph create/import only records shape and a debug name. Later GPU allocation and Handle→RHI need a CPU Frame graph resource desc on every resource. This slice stores that desc at Setup so compile can reject illegal numbers before any device work.

## What Changes

- **BREAKING:** GraphBuilder `createTransient` and `importExternal` take a required Frame graph resource desc (same type). Shape stays a separate argument. Texture fields: Frame graph format, width, height, sample count, mip count. Buffer field: size in bytes. Clear, usage, host-visible, array/3D/cube are not on the desc.
- Frame graph format is an enum on the graph (`Undefined`, `R8G8B8A8_UNORM`, `D32_SFLOAT`), not `VkFormat` and not `rhi::PixelFormat`. Buffer has no format.
- `FrameGraph::resourceDesc(handle)` returns the stored desc without compile. DCE does not erase it. A never created/imported handle returns a default-constructed desc and does not throw.
- Compile fails with `InvalidDesc` when a created or imported desc is illegal (Texture: `Undefined` format, width or height 0, sample or mip count 0; Buffer: size 0), even if DCE would drop that resource. Create/import still accept the desc and do not throw.
- Failure reason priority: `InvalidPass`, then `InvalidDesc`, then `DanglingAccess`, then `NoSink`, then `Cycle`. After `InvalidDesc`, `execute()` is `NotCompiled`.
- Extend [ADR 0061](../../../docs/adr/0061-frame-graph-cpu-setup.md). Do not write ADR 0062.
- First-party `frame_graph_test` covers the five User stories. Existing compile/execute cases pass a legal desc. Still no handle→RHI resolve, GPU allocation, aliasing, barriers, or `ForwardRenderPath` replacement.

## User stories

1. I create a Transient Texture (`R8G8B8A8_UNORM`, 8×8, samples 1, mips 1) and a Sink that Writes it: compile succeeds; `resourceDesc` matches that desc before compile; the resource stays Transient.
2. I import an External Texture with the same viewport-color stand-in desc and a Sink that Writes it: compile succeeds; the resource stays External; `resourceDesc` matches; the graph does not treat it as a Transient it would allocate.
3. I create a Transient Buffer (size 256), write it, then read it from a Sink: compile succeeds; `resourceDesc` size is 256.
4. I create an illegal desc (Texture: `Undefined` / width or height 0 / samples or mips 0; or Buffer size 0) even when no Pass uses it: compile fails with `InvalidDesc`, does not throw, and the live order is empty.
5. The same graph has a never-added Pass handle, an illegal desc, and a dangling access: compile reports only `InvalidPass`. With the illegal Pass removed, illegal desc plus dangling reports `InvalidDesc`. After `InvalidDesc`, `execute()` is `NotCompiled`. Clear→Forward Sink and DCE still work when every desc is legal.

## Capabilities

### New Capabilities

- *(none)*

### Modified Capabilities

- `frame-graph`: create/import require a Frame graph resource desc; `resourceDesc` query; compile `InvalidDesc` with the locked priority; still no RHI.

## Impact

- **Engine:** `engine/src/runtime/function/render/frame_graph/` desc types, GraphBuilder create/import signatures, `resourceDesc`, compile `InvalidDesc`. No RHI includes. Not a Context System.
- **Tests:** extend `engine/src/tests/frame_graph_test.cpp`. Existing Setup calls take a legal desc. Link stays `engine_runtime` only.
- **Docs:** CONTEXT Frame graph resource desc / format / compile (Grill); ADR 0061 addendum. No ADR 0062.
- **Non-goals:** resolving handles to RHI; GPU allocation; memory aliasing; barriers; command recording; replacing ForwardRenderPath; JSON; Blackboard; Frame arena; attaching a device object on import; expanding `rhi::PixelFormat`; SSAO/outline/pick formats; Job System scheduling compile; ADR 0062.
