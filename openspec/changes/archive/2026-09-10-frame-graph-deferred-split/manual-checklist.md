# Manual checklist — frame-graph-deferred-split

One row per User story. Agent QC does not substitute. Only the human confirms.

**Status:** Not run

Stories 1–4 are windowed editor (Test Project). Stories 1 and 3 need `BLUNDER_EDITOR_DEFERRED=1`. Story 2 is default Forward (env unset) plus Player. Story 5 is Headless (`frame_graph_test`).

| # | User story | Pass |
|---|------------|------|
| 1 | `BLUNDER_EDITOR_DEFERRED=1`：Viewport 看起来和现在一样。G-buffer 和 lighting 是两个 Pass，不是一个 `renderFrame` Scene。Shadow 仍在 G-buffer 回调里。grid / transparent 仍在 Lighting 的 LOAD 里。Copy 仍是 Sink。Overlays 仍在 Lighting 之后。 | |
| 2 | 环境变量未设（以及 Player）：仍是一个 Scene Pass，Forward `renderFrame`。没有 G-buffer / Lighting Pass。看起来和现在一样。 | |
| 3 | Deferred 且没有选中、没有 line overlay、没有 SSAO：那些 overlay Pass 仍不建。G-buffer 和 Lighting **仍建**。Copy 仍跑。 | |
| 4 | Camera Preview、Mesh Preview、Scene Thumbnail 仍 `ForwardRenderPath::renderFrameTo`。Preview 仍在这次 viewport `execute` 之后。 | |
| 5 | `frame_graph.h` 仍无 `vulkan.h`。`frame_graph_test` 仍 Dummy、仍无 Vulkan device。Dummy 证明：live 顺序 G-buffer → Lighting → Copy；Copy 是 Sink；G-buffer 不是；Lighting 前没有 `from` DepthAttachment、`to` Sampled 的 Dummy barrier。 | |
