## 1. Viewport graph Setup

- [x] 1.1 Rename `recordViewportSceneGraph` to `recordViewportGraph`. Scene Pass: Write color ColorAttachment then Read Sampled; Write depth DepthAttachment then Read Sampled; Read shadow Sampled. Do not `markSink` Scene. Keep per-tick reconstruct, fail-closed External-only allocate, Vulkan recorder bind, fatal compile/allocate/plan/execute. Do not import extra images. Do not expand `FrameGraphFormat` or `FrameGraphUsage`.
- [x] 1.2 Add post-Scene Passes on that same graph, with today’s `tickVulkan` `if`s: Outline when `hasActiveOutline()`; Line+AA when `hasActiveLineOverlays()` (one Pass; always declare the color handshake when built, including when AA is off); SSAO when `ssao_enabled` (first depth access Sampled); Screen overlays every frame when `OverlaySystem` exists; Copy always, last, only Sink, Read color Sampled. Color-writing overlay Passes declare Read Sampled then Write ColorAttachment then Read Sampled. Callbacks call today’s `draw_outline` / `draw_overlay_lines` (+ optional `draw_overlay_aa`) / `SsaOPass::apply` / `draw_screen_overlays` / zero-copy `transitionToShaderRead` or CPU copy then shader-read. Remove those recordings from after `execute`. Leave `begin_sync` before Setup. Leave Camera Preview after this `execute`. Do not put Preview / Thumbnail / GPU pick on this graph. Do not add empty Passes for omitted stages. Do not keep a leftover overlay branch as fallback.

## 2. Tests

- [x] 2.1 In `frame_graph_test`, Dummy-execute a Scene (Write ColorAttachment then Read Sampled) plus overlay (Read Sampled then Write ColorAttachment then Read Sampled) plus Copy Sink (Read Sampled). Assert no Dummy barrier whose `before` is the overlay Pass has ColorAttachment as `to` usage. No Vulkan device.
- [x] 2.2 Dummy-compile Scene plus Copy Sink with no overlay Pass: live order is Scene then Copy; Copy is Sink; Scene is not. Dummy-execute a graph with an unreachable overlay Pass: that callback does not run. Keep `frame_graph_test` Dummy and device-free. Confirm it still builds and passes (`build/vs2026-debug`, Debug).

## 3. Docs

- [x] 3.1 Keep [ADR 0068](../../../docs/adr/0068-frame-graph-viewport-overlays.md) aligned with the shipped overlay Passes and Copy Sink. CONTEXT: Frame graph pass / viewport Sink (Copy, not Scene). Point [ADR 0067](../../../docs/adr/0067-frame-graph-viewport-wire.md) / [ADR 0062](../../../docs/adr/0062-deferred-render-path.md) / [ADR 0066](../../../docs/adr/0066-frame-graph-gpu-execute.md). Update `docs/agents/render-pipeline.md` so post-Scene stages sit inside Frame graph execute.
