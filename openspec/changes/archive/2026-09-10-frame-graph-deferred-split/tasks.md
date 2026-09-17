## 1. Viewport graph Setup

- [x] 1.1 Split `DeferredRenderPath::renderFrame` into two record methods: G-buffer callback = today’s `recordShadowPass` plus G-buffer RP; Lighting callback = today’s lighting RP plus LOAD (`cmdBarrierForLoadPass`, `beginLoadRenderPass`, `recordSceneOverlayAndTransparent`, `endLoadRenderPass`). Do not leave `renderFrame` as a Scene callback. Do not import G-buffer images. Do not Transient-allocate them. Do not expand `FrameGraphFormat` or `FrameGraphUsage`.
- [x] 1.2 In `recordViewportGraph`: env off or Player keeps `viewport.scene` + Forward `renderFrame` with today’s Scene accesses. Editor Deferred: do not add `viewport.scene`; add `viewport.gbuffer` then `viewport.lighting` every Deferred tick (even with no opaques). G-buffer: Write depth DepthAttachment then Read Sampled; no color; do not declare shadow. Lighting: Read depth Sampled; Read shadow Sampled; Write color ColorAttachment then Read Sampled. Neither is a Sink. Overlay/Copy Passes stay after Lighting (Forward: after Scene). Same overlay `if`s. Copy stays last and only Sink. Failed execute stays fatal. Do not add Fake empty G-buffer/Lighting Passes on Forward. Do not keep a leftover `renderFrame` Scene fallback. Leave Camera Preview after this `execute`.

## 2. Tests

- [x] 2.1 In `frame_graph_test`, Dummy-execute External color+depth (Lighting Reads shadow Sampled): G-buffer Write DepthAttachment then Read Sampled (no color); Lighting Read depth Sampled, Write color ColorAttachment then Read Sampled, Read shadow Sampled; Copy Sink Read color Sampled. Assert live order G-buffer → Lighting → Copy; Copy is Sink; G-buffer is not; no Dummy barrier whose `before` is Lighting, `from` usage is DepthAttachment, and `to` usage is Sampled. Keep existing overlay handshake Dummy. Keep `frame_graph_test` Dummy and device-free. Confirm it still builds and passes (`build/vs2026-debug`, Debug).

## 3. Docs

- [x] 3.1 Bump [ADR 0069](../../../docs/adr/0069-frame-graph-deferred-split.md) to accepted. Keep CONTEXT Frame graph pass / Deferred Render Path / G-buffer / Sink pointers. Point [ADR 0062](../../../docs/adr/0062-deferred-render-path.md) / [ADR 0068](../../../docs/adr/0068-frame-graph-viewport-overlays.md). Update `docs/agents/render-pipeline.md` so editor Deferred is G-buffer then Lighting Passes, not one Scene wrapping `renderFrame`.
