# Manual checklist — frame-graph-viewport-overlays

One row per User story. Agent QC does not substitute. Only the human confirms.

**Status:** Human accepted (2026-09-10)

Stories 1–4 are windowed editor (Test Project). Story 3 needs `BLUNDER_EDITOR_DEFERRED=1`. Story 5 is Headless (`frame_graph_test`).

| # | User story | Pass |
|---|------------|------|
| 1 | 默认编辑器 Viewport：outline、gizmo、SSAO、copy 进 UI 看起来和现在一样。这些阶段是同一张 Frame graph 上的 Pass，不是 `execute` 之后写死的顺序。Copy 是 Sink。 | |
| 2 | 没有选中、没有 line overlay、没有 SSAO：那些 Pass 不建（没有 line 就整段 Line+AA Pass 都不建）。Viewport 仍正确。Copy 仍跑。 | |
| 3 | `BLUNDER_EDITOR_DEFERRED=1`：Scene 仍是一个 `DeferredRenderPath::renderFrame` 回调；G-buffer 不上 graph；outline / gizmo 仍不进 G-buffer；overlay / copy 仍在这张 graph 上。 | |
| 4 | Camera Preview、Mesh Preview、Scene Thumbnail 仍 `ForwardRenderPath::renderFrameTo`。Preview 仍在这次 viewport `execute` 之后。 | |
| 5 | `frame_graph.h` 仍无 `vulkan.h`。`frame_graph_test` 仍 Dummy、仍无 Vulkan device。Dummy 证明：color-writing overlay 回调前没有 ColorAttachment graph barrier；省略的 Pass 不是 live；Copy 是 Sink。 | |
