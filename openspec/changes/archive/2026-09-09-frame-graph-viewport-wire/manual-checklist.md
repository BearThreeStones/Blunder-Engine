# Manual checklist — frame-graph-viewport-wire

One row per User story. Agent QC does not substitute. Only the human confirms.

**Status:** Human accepted (2026-09-09)

Stories 1–4 are windowed editor (Test Project). Story 2 needs `BLUNDER_EDITOR_DEFERRED=1`. Story 5 is Headless (`frame_graph_test` plus recorder compilation-unit / mapping test).

| # | User story | Pass |
|---|------------|------|
| 1 | 不设 `BLUNDER_EDITOR_DEFERRED`，打开编辑器看 Viewport：网格仍 Forward 画进 offscreen，Slint 仍是同一套像素。这一帧的 Scene 是 Frame graph `execute`，不是 `tickVulkan` 里直接 `forward_path->renderFrame`。 | |
| 2 | `BLUNDER_EDITOR_DEFERRED=1`：opaque 仍 G-buffer 再 lighting 进同一张 offscreen；transparent 仍 lighting 之后的 Forward；scene overlay 不进 G-buffer。Scene Pass 回调是 `DeferredRenderPath::renderFrame`，没有第二条绕过 graph 的 `tickVulkan` 分支。 | |
| 3 | Camera Preview、Mesh Preview、Scene Thumbnail 仍 `ForwardRenderPath::renderFrameTo`。Player 仍 Forward，不读编辑器 deferred 开关。 | |
| 4 | Scene `execute` 之后仍是 outline → overlay lines → overlay AA → SSAO → screen overlays → copy/readback。观感与现在同一刀前一致。 | |
| 5 | `frame_graph.h` 仍无 `vulkan.h`。`frame_graph_test` 仍无 Vulkan device、仍 Dummy recorder。Vulkan recorder 是别的编译单元。 | |
