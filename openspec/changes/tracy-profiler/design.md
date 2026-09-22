# Design

## Context

See proposal.md for why. Grill locked 2026-09-20: (1) HUD = Slint overlay on editor Viewport **and** windowed Player, same numbers; Headless/MCP none; ship HUD on, Tracy off. (2) Layer 2 = in-editor self-drawn Slint dock (frame strip / lanes / zone inspect) on an engine ring; forbid Tracy View / ImGui / egui / Qt / WebView as a Slint component. (3) Tracy Client optional dual-write to standalone `Tracy.exe` only, not the panel source; dev `TRACY_ENABLE` + `TRACY_ON_DEMAND` + localhost; CI/ship no `TRACY_ENABLE`. Decision: [ADR 0075](../../../docs/adr/0075-frame-timing-hud-and-tracy.md).

Today: `HighPrecisionTimer` + EMA FPS (`calculateFPS`). Player without Slint writes `"… N FPS"` into the title; editor title has no FPS. **Zero** `VkQueryPool` / `vkCmdWriteTimestamp` / `VK_EXT_calibrated_timestamps`. `.gitmodules` declares `engine/3rdparty/tracy` gitlink `460352d`; worktree empty; CMake has no `add_subdirectory`. Nsight: 12 s idle ≈ 240 engine ticks vs 3 Skia Presents — frame boundary **must** be `tickOneFrame`. Viewport graph already names Passes (`viewport.gbuffer` / `lighting` or Player `viewport.scene`, optional `outline` / `line_aa` / `ssao` / `volumetric_fog` / `screen`, Sink `viewport.copy`). Product Project: `E:\Blunder Projects\DogWalk`. Do not edit `cursor/scene-collision-bridge-8a89` / PR 24 / PR 36 / PR 37.

## Goals / Non-Goals

**Goals:**

- HUD + editor dock on an engine-owned timestamp/ring path that works with Tracy compiled out.
- Optional Tracy Client dual-write to `Tracy.exe` (on-demand, localhost).
- Windowed Player HUD-only Slint + shared-device Present.
- Pass-level GPU zones; `FrameMark` at tick; discrete-GPU log.

**Non-Goals:**

- Tracy UI as a Slint component; embed Server; Insights; per-draw GPU zones.
- Changing Present / idle pacing; converting Player to deferred; product settings UI.
- PR 24 / 36 / 37.

## Decisions

1. **Two product layers; Tracy is not a layer.**  
   HUD = plan line “editor can see / install package playable FPS.” Dock = in-editor history + lanes. Tracy Client is a **dev overlay** on the same instrumentation. Shipping ticks the plan line with HUD (and editor ring dock), not port 8086.  
   *Alternatives:* Tracy-only (human rejected); HUD-only with timeline in `Tracy.exe` (does not satisfy “editor can see” Layer 2).

2. **HUD is Slint overlay; dock is Slint bottom dock.**  
   Editor HUD sits on the existing Viewport (occupancy-heatmap session toggle: default off, not persisted, covers the 3D view only). Player HUD is the same readout without `editor_window.slint`. Dock copies Animation Window / Console: `EditorTheme`, bottom dock, `Rectangle` + `VectorModel` (fallback `SharedPixelBuffer` if zone count blows the retained tree).  
   *Alternatives:* ImGui / egui / Qt / WebView / Tracy View (`TRACY_NO_ROOT_WINDOW` is ImGui-in-ImGui). Forbidden.

3. **HUD and dock read the engine ring, never Tracy internals.**  
   One ring slot per `tickOneFrame`: CPU dt, GPU frame ms, named Pass GPU ms, a few CPU zones (tick / Job / scene sync), packed instance / MeshBatch / surviving meshlet-cmd counts plus lights. Capacity **~120** frames. Panels bind this ring. Tracy has no `getLastFrames()`.  
   *Alternatives:* parse Tracy protocol / `.tracy` (forbidden); puffin in-process buffer (Rust + egui).

4. **Engine query pool + Tracy’s pool when Client is on.**  
   HUD/ring use the engine `VkQueryPool` (`vkCmdWriteTimestamp`). When `TRACY_ENABLE`, `TracyVkZone` uses Tracy’s own pool. Dual timestamp cost is accepted so shipping does not link Client. Calibrated timestamps (`VK_EXT_calibrated_timestamps` + `VK_TIME_DOMAIN_QUERY_PERFORMANCE_COUNTER_EXT` on Windows) feed `TracyVkContextCalibrated` / HostCalibrated when present; missing → ordinary `TracyVkContext`, HUD still shows relative GPU ms, long Tracy captures may drift.  
   *Alternatives:* HUD reads Tracy collect (breaks shipping); one shared pool (fights Tracy’s context API).

5. **`FrameMark` = `tickOneFrame`, not Present.**  
   Optional `FrameMarkNamed("slint-present")` around real Skia Present (idle almost never). Do not disable idle pacing or treat swapchain as the frame.  
   *Alternatives:* mark Present (Nsight already showed that is not engine FPS).

6. **Tracy Client: on-demand, localhost, `engine_runtime` only.**  
   CMake `option(TRACY_ENABLE OFF)`. Presence of the macro enables capture (`TRACY_ENABLE=0` is **not** off). Also define `TRACY_ON_DEMAND`, `TRACY_ONLY_LOCALHOST`, `TRACY_NO_BROADCAST` when enabling. Bump gitlink to tag **v0.14.1**. Do not compile Tracy GUI. Do not add Client to SHARED `blunder_engine_c.dll`. Job workers `tracy::SetThreadName` when Client is on.  
   *Alternatives:* always-on Client (unconnected events grow to GB); embed Server (manual § embedding; global allocator + ImGui vs Skia).

7. **GPU zones = named Passes + few internals.**  
   Same names on HUD table, ring, and Tracy. Internals: shadow fill, GPU-driven cull, froxel fill, lighting fullscreen triangle — not 5000 MeshRenderers, not 138 Spot GPU zones. 138 / 5000 use **count plots** + lighting/scene GPU ms. Zones may record on SECONDARY; Collect stays PRIMARY, after `execute`, before submit, outside a render pass. Zone ctor/dtor inside `vkBegin/EndCommandBuffer`.  
   *Alternatives:* per-draw zones (forbidden); wait for VRS deferred Player (this knife timestamps Forward `viewport.scene` on Player).

8. **Windowed Player: HUD-only Slint + shared Vulkan Present.**  
   Replace `SdlViewportSink` SDL_Renderer CPU blit. 3D stays offscreen; Slint composites the color target + HUD, same shared-device family as the editor. Not `editor_window.slint`. Not start/settings/pause. Headless Player: no window, no HUD, keep CPU readback Play frames. Title-bar FPS MAY remain.  
   *Alternatives:* title-only (Grill: not the only readout); ImGui overlay; hang the editor dock on Player.

9. **HUD/dock session toggles.**  
   Default **off**, not persisted, not Project settings. **F3** (F11 is RenderDoc). Editor Viewport menu row next to Froxel Occupancy Heatmap. Player same hotkey. Dock default off.  
   *Alternatives:* default on (pollutes viewport captures); persist (ADR 0062: no profiler settings page).

10. **Discrete GPU is the budget device.**  
    Keep DISCRETE +1000 scoring. Log `deviceName` + `deviceType` at select. Non-DISCRETE: HUD “GPU timings unreliable”; do not use iGPU numbers for 138 Spot. Close MCP before a windowed timing session (existing dual-Vulkan crash). Do not time with RenderDoc F11. Acceptance: Windows 3050. Linux CI / lavapipe GPU ms are not a gate.  
    *Alternatives:* require DISCRETE (breaks Linux CI device create).

11. **Independent of collision-bridge, open-progress, and VRS git.**  
    Branch from `main` `3e3d42d`. No files from those PRs. ADR **0075** because 0073 is open-progress and 0074 is VRS.  
    *Alternatives:* stack on PR 24 / 36 / 37 (forbidden).

## Risks / Trade-offs

- **[Risk] Dual timestamp pools when Client is on** → Extra GPU queries. Accepted; keeps shipping independent of Tracy.
- **[Risk] Player present rewrite** → Windowed Player moves from SDL_Renderer blit to Slint shared-device Present. Mitigate: Headless path untouched; keep title FPS; no editor shell widgets.
- **[Risk] Intel iGPU timestamps** → HUD warns; budget walks stay on 3050.
- **[Risk] Unconnected Tracy without ON_DEMAND** → Must define `TRACY_ON_DEMAND` whenever `TRACY_ENABLE` is on.
- **[Risk] Zone scope outside command buffer** → Spec Collect/zone lifetime; tests cannot catch GPU scope; apply checklist.
- **[Trade-off] ~120-frame ring** → Enough for a strip; not Insights history.
- **[Trade-off] RelWithDebInfo for budget** → Pure Debug+validation is not the budget build (Tracy manual).

## Migration Plan

1. Land planning artifacts (this change). No HUD / dock / Tracy C++ yet.
2. On apply: query pool + ring; HUD; dock; Player HUD-only Slint present; optional Client + v0.14.1; tests; docs.
3. Human walk on Windows DogWalk `se-world.scene.asset` in windowed `engine_editor` and Play. Do not merge this planning PR into PR 24, 36, or 37.
4. Rollback: drop HUD/dock/ring/query; restore Player SDL_Renderer blit; leave Tracy submodule unwired (`TRACY_ENABLE` off).

No content format migration.

## Open Questions

None that block specs. Grill defaults locked 2026-09-20 (Player HUD host, v0.14.1, HUD default off, F3, HUD+TracyPlot counts, calibrated-if-present, dock default off / ~120 frames).
