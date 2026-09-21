# Tasks

## 1. Engine ring and GPU timestamps

- [x] 1.1 Add an engine GPU timestamp query pool and a ~120-frame Frame timing ring (CPU dt, GPU ms, named Passes, tick/Job/scene-sync CPU zones, instance/light/draw counts), and verify a unit test wraps the ring and reports deltas with Tracy Client compiled out
- [x] 1.2 Write timestamps on live viewport Passes (`viewport.gbuffer`/`lighting` or Player `viewport.scene`, plus `ssao` / `volumetric_fog` / `copy`) and a few internals (shadow / cull / froxel / lighting triangle), and verify the ring has those names and does **not** emit a GPU zone per draw or per Spot
- [x] 1.3 Log selected `deviceName` + `deviceType` at physical-device pick, and verify a non-DISCRETE selection is observable in the log without failing device create

## 2. HUD and Profiler dock (Slint)

- [x] 2.1 Add the Frame timing HUD Slint overlay (FPS, CPU ms, GPU ms, Pass table, instance/light counts) on the editor Viewport, default off, F3 + Viewport menu, not persisted, and verify a fresh launch has no overlay and F3 toggles it without a settings page
- [x] 2.2 Add the editor Profiler dock (frame strip, thread/GPU Pass lanes, click zone name+ms) as a bottom Editor Theme dock bound to the ring, and verify it works with no Tracy.exe and does not appear in Player
- [x] 2.3 When the selected device is not DISCRETE, mark GPU timings unreliable on the HUD, and verify the warning is visible with the HUD on

## 3. Windowed Player HUD present

- [x] 3.1 Replace windowed Player `SdlViewportSink` SDL_Renderer blit with a HUD-only Slint root on the shared Vulkan device (3D still offscreen), and verify Headless Player still has no OS window and still sends Play frames
- [x] 3.2 Mount the same Frame timing HUD on that Player root (F3, default off, no Profiler dock, title FPS MAY remain), and verify the HUD is the timing readout and editor shell widgets are absent

## 4. Optional Tracy Client dual-write

- [x] 4.1 Bump `engine/3rdparty/tracy` to tag v0.14.1, CMake `option(TRACY_ENABLE OFF)` + `add_subdirectory` linked only to `engine_runtime`, with `TRACY_ON_DEMAND` + `TRACY_ONLY_LOCALHOST` + `TRACY_NO_BROADCAST` when enabled, and verify Merge CI / default configure do **not** define `TRACY_ENABLE` and SHARED `blunder_engine_c` does not compile a second `TracyClient.cpp`
- [x] 4.2 When Client is on: `ZoneScoped` + `FrameMark` at `tickOneFrame`, Pass-level `TracyVkZone`, Collect after graph `execute` before `vkQueueSubmit` on PRIMARY outside a render pass, `TracyPlot` for instance/light/draw, Job worker `SetThreadName`, calibrated Tracy VK context when `VK_EXT_calibrated_timestamps` exists, and verify an unconnected process does not grow unbounded event memory
- [x] 4.3 Do not change idle/interactive Present pacing; optional `FrameMarkNamed("slint-present")` only around real Skia Present, and verify idle Present still throttles while the ring still ticks

## 5. Hosts, DogWalk contract, docs (apply later; not this planning commit)

- [x] 5.1 Skip HUD, dock, and Tracy listen on Headless / `--mcp` / Linux Merge CI, and verify those hosts still start
- [x] 5.2 Human walk remains DogWalk `assets/Scenes/se-world.scene.asset` in windowed `engine_editor` and Play (not Test/Sponza, not collision QC `root.scene.asset`); checklist stays **Not run** until the human walks it
- [x] 5.3 Do not edit `cursor/scene-collision-bridge-8a89`, PR 24, PR 36, or PR 37; verify this change’s diff has no files from those branches
- [x] 5.4 Keep `CONTEXT.md` Frame timing / Tracy terms aligned with implementation names; ADR 0075 stays the HUD+dock+dual-write record; on apply update `docs/agents/render-pipeline.md`
- [x] 5.5 `openspec validate tracy-profiler --strict` passes
