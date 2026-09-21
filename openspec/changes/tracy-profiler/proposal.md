# Proposal

## Why

DogWalk’s 2026-10 “forest stands” line needs a **basic profiler** (CPU / GPU frame time, main Passes, readable in the editor, later 5000 instances / 138 Spot and the install package) — **not** Insights. Today the engine has only `HighPrecisionTimer` + EMA FPS, **zero** `VkQueryPool`, and a declared-but-unwired Tracy gitlink. A Tracy window alone cannot tick that plan line. Decision: [ADR 0075](../../../docs/adr/0075-frame-timing-hud-and-tracy.md).

## What Changes

- **Layer 1 — Frame timing HUD.** Slint overlay on the windowed editor Viewport **and** windowed Player. Same numbers: FPS, CPU ms, GPU ms, main Pass table, instance / light counts. Default **off** (F3 + editor Viewport menu, occupancy-heatmap rhythm). Not persisted. Headless / MCP / Project Manager have **no HUD**. Shipping: HUD on, Tracy off.
- **Layer 2 — editor Profiler dock.** Self-drawn Slint bottom dock (Animation Window / Console rhythm): frame strip, thread / GPU Pass lanes, click a zone for name + ms. Backend is the **Frame timing ring** (~120 frames). Player has **no** dock. **Forbid** Tracy View / ImGui / egui / Qt / WebView as a Slint component. Not Insights.
- **Engine timestamps + ring, independent of Tracy.** Path-owned GPU timestamp query pool + CPU dt + readable ring. HUD and dock bind **only** that ring. Shipping with no Client still shows FPS / frame times / Pass table / strip.
- **Tracy Client optional dual-write.** Same instrumentation writes `ZoneScoped` / `TracyVkZone` / `TracyPlot` **and** the ring. Standalone `Tracy.exe` only. Client is **not** the panel source. Dev: `TRACY_ENABLE` + **`TRACY_ON_DEMAND`** + `TRACY_ONLY_LOCALHOST` + `TRACY_NO_BROADCAST`. CI / shipping: **do not define** `TRACY_ENABLE` (`=0` is a no-op). Do not ship `Tracy.exe`. Pin submodule to tag **v0.14.1**. Link Client into `engine_runtime` only — not a second `TracyClient.cpp` in SHARED `blunder_engine_c.dll`.
- **`FrameMark` = `tickOneFrame`.** Not Present. Optional `FrameMarkNamed("slint-present")` around real Skia Present. Do not change idle pacing.
- **GPU granularity = named Passes + a few internals.** Live graph names: `viewport.gbuffer` / `viewport.lighting` or Player `viewport.scene`, plus `ssao` / `volumetric_fog` / `copy`; internals shadow / cull / froxel / lighting fullscreen triangle. Never per-draw / 5000 instance / 138 Spot GPU zones. `TracyVkCollect` after graph `execute`, before `vkQueueSubmit`, PRIMARY, outside a render pass.
- **Windowed Player HUD host.** HUD-only Slint root + shared Vulkan device Present (editor zero-copy family). Replaces windowed `SdlViewportSink` SDL_Renderer blit. Not the editor shell. Not the 2027-01 game shell. Headless Player stays windowless, no HUD.
- **Discrete GPU log.** Startup logs selected `deviceName` + `deviceType`. Non-DISCRETE: HUD marks GPU timings unreliable. Acceptance on Windows 3050. Linux CI GPU numbers are not a budget gate.

**Out of scope:** Tracy View / ImGui / egui / Qt / WebView as Slint; embed Tracy Server; puffin / Optick / easy_profiler / microprofile / Remotery / `slint-ui-system`; HUD or dock reading Tracy buffers / protocol / `.tracy`; per-mesh / per-instance / per-Spot GPU zones; C# / CoreCLR / lock map / memory pool / hardware PMU; Nsight Frame duration as `FrameMark` proof; RenderDoc+timer as acceptance; Present / idle pacing; Mesh Loader / flatten / open overlay; converting Player / Preview / Thumbnail to deferred (VRS [#37](https://github.com/BearThreeStones/Blunder-Engine/pull/37)); 2027-01 game shell; Linux CI GPU budget; product settings UI; `cursor/scene-collision-bridge-8a89` / [#24](https://github.com/BearThreeStones/Blunder-Engine/pull/24); [#36](https://github.com/BearThreeStones/Blunder-Engine/pull/36); [#37](https://github.com/BearThreeStones/Blunder-Engine/pull/37).

## User stories

1. Open DogWalk `se-world.scene.asset` in windowed `engine_editor`. The Viewport can show a thin HUD: CPU frame ms, GPU frame ms, main Pass table, instance / light counts. Default off; F3 or Viewport menu toggles it. Off returns today’s clean viewport. This is Layer 1. Opening Tracy is not “editor can see.”
2. The same windowed editor can open a **self-drawn Slint dock** (bottom, Animation Window rhythm): frame strip, thread / GPU Pass lanes, click a zone for name and ms. Data is the engine ring. Closing the dock leaves the viewport clean. Not ImGui. Not Tracy View inside the shell.
3. The same **dev** build can launch `Tracy.exe` to `127.0.0.1` (default 8086; another port if busy). Sampling starts only when connected; disconnected does not grow to GB. The timeline shows `tickOneFrame` `FrameMark` and Frame graph Pass zones. HUD and dock still have numbers with no connection. The outer window is optional deep dive, not the panel backend.
4. Windowed `engine_player` on the same scene has **no** editor dock, but the same Slint HUD (togglable). Title-bar FPS may remain; it is not the only readout. Walk the forest and read frame time from the HUD. Tracy is not required. No Layer 2 dock.
5. Shipping / install build: HUD still opens (FPS + frame times + main Passes). The editor dock still binds the ring (read-only). The process does not listen on a profiler port, does not ship `Tracy.exe`, and does not define `TRACY_ENABLE`.
6. Headless / `--mcp` / Linux Merge CI have no HUD, no dock, and no Tracy listen. Cloud lavapipe GPU numbers are not acceptance.
7. Discrete-GPU machines log the selected `deviceName`. Integrated GPU: HUD warns GPU timings unreliable. The walk is DogWalk `se-world.scene.asset`. Test and Sponza are not the demo. 138 Spot, the dog walking, and a VRS mask are not required. Do not touch PR 24, PR 36, or PR 37.

## Capabilities

### New Capabilities

- `tracy-profiler`: Two product layers (Frame timing HUD + editor Profiler dock) on an engine Frame timing ring and GPU timestamp queries. Optional Tracy Client dual-write to standalone `Tracy.exe` only. `FrameMark` at `tickOneFrame`. Pass-level GPU zones. Dev `TRACY_ENABLE` + `TRACY_ON_DEMAND` + localhost; CI/shipping never define `TRACY_ENABLE`. HUD default off (F3). No second UI toolkit. No Insights.
- `dogwalk-tracy-profiler`: Human walk on DogWalk `assets/Scenes/se-world.scene.asset` in windowed `engine_editor` and Play. HUD + dock without Tracy.exe; optional Tracy connect. Not Test/Sponza. Not collision QC `root.scene.asset`. Not PR 24 / 36 / 37.

### Modified Capabilities

- `play-player`: Windowed Player presents 3D through a HUD-only Slint root (shared Vulkan device), not `SdlViewportSink` CPU blit. Same Frame timing HUD as the editor Viewport. No Profiler dock. No editor shell. Headless Player still has no OS window and no HUD. Editor Overlays stay off. Title-bar FPS MAY remain; it SHALL NOT be the only readout.
- `headless-host`: Headless Editor / Player / `--mcp` SHALL NOT mount the Frame timing HUD, Profiler dock, or a Tracy listen socket.
- `frame-graph-viewport`: Named viewport Passes SHALL record engine GPU timestamps (and, when the Client is compiled in, matching `TracyVkZone`). `TracyVkCollect` (when enabled) SHALL run after `execute`, before `vkQueueSubmit`, on PRIMARY, outside a render pass. Granularity SHALL stay Pass + few internals, never per-draw.
- `editor-viewport-pacing`: Frame boundary for HUD / ring / `FrameMark` SHALL be `tickOneFrame`, not Skia/SDL Present. Idle / interactive Present pacing SHALL NOT change for this profiler.

## Impact

- **Engine:** Query pool + ring beside Render; Pass-level timestamp writes; Collect after viewport execute; device log `deviceName`/`deviceType`; Job workers `tracy::SetThreadName` when Client is on; CMake `option(TRACY_ENABLE OFF)` + `add_subdirectory(tracy)` into `engine_runtime` only; bump `engine/3rdparty/tracy` gitlink to v0.14.1.
- **UI:** Viewport HUD overlay (editor + Player); editor bottom Profiler dock (`EditorTheme`, Animation Window family); F3; Viewport menu row. English. Not persisted. No product settings page (ADR 0062).
- **Player present:** Windowed Player drops SDL_Renderer blit for HUD-only Slint + shared-device Present. Headless unchanged.
- **Tests:** Ring wrap / capacity; timestamp delta without a Tracy Client; host gates (Headless no HUD; CI compile without `TRACY_ENABLE`); CMake does not define the macro on Merge CI. Windowed HUD/dock/`Tracy.exe` is Human acceptance on Windows 3050.
- **Docs:** CONTEXT Frame timing / Tracy terms; [ADR 0075](../../../docs/adr/0075-frame-timing-hud-and-tracy.md). Apply later updates `docs/agents/render-pipeline.md`.
- **This planning change** is OpenSpec artifacts + glossary/ADR only — no engine/Slint C++ until `/opsx:apply`.
