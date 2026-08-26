## Context

See proposal.md. Grilling locked CONTEXT Headless and [ADR 0043](../../../docs/adr/0043-host-observation.md). `EngineHostMode` is Editor or Player only. `RuntimeGlobalContext::startSystems` always creates `WindowSystem` (SDL window). Editor then mounts UiHost, Slint, viewport sink/bridge; Player skips Slint but still has a window and presents. Capture already uses the Scene still GPU path (no HWND). Play frame v1 CPU-readbacks `RenderSystem` offscreen (`capturePlayProcessFrame`). `PlaySessionController` is created only in the windowed Editor branch. Vulkan today takes a `VkSurfaceKHR` from the SDL window.

## Goals / Non-Goals

**Goals:**
- `--headless` on `engine_editor` and `engine_player`; store as a bool beside `EngineHostMode`, not a third enum value
- Headless Editor: skip SDL window, Slint, UiHost, viewport sink/bridge; still Authorship + PlaySessionController + Scene still / Capture
- Headless Player: skip SDL window and swapchain present; keep offscreen color target at Capture aspect; reuse v1 Play frame readback
- Headless Editor Play spawns Headless Player (`--headless` on the child argv)
- Tests can boot both hosts without an OS window

**Non-Goals:**
- CLI / MCP adapters
- Play dump
- Hidden `SDL_WINDOW` as Headless
- Changing Capture 16:9 / 1280 or Play step dt 1/60
- Headless Project Manager
- Making windowed Editor spawn a Headless Player

## Decisions

### D1 — Flag beside host mode
**Choice:** `bool headless` (launch `--headless`) plus existing `EngineHostMode`. `startEngine` / `startSystems` take both.
**Why:** ADR 0043: no `EngineHostMode::Headless`, no `engine_agent`.
**Rejected:** Third enum; separate `engine_agent` exe.

### D2 — No SDL window, no hidden HWND
**Choice:** Headless does not call `WindowSystem::initialize` (no `SDL_CreateWindow`). Vulkan instance/device without `VkSurfaceKHR` / swapchain. Offscreen target sized to Capture extent (1280×720) for Headless Player present-skip.
**Why:** CONTEXT: no OS window. Hidden window is still a window.
**Rejected:** `SDL_WINDOW_HIDDEN`; dummy 1×1 HWND for the swapchain.

### D3 — Skip shell, keep Authorship and Play session
**Choice:** Headless Editor skips UiHost, Slint, viewport sink/bridge. Still mounts Authorship System, Document History, Scene, Content Browser (no UI), thumbnail/Capture GPU, `PlaySessionController`.
**Why:** CONTEXT Headless Editor omits those three; still Editor.
**Rejected:** Skipping Authorship; requiring Slint for Capture.

### D4 — Headless Editor spawns Headless Player
**Choice:** `buildPlayerSpawnArgv` adds `--headless` when the editor session is Headless. Windowed Play stays windowed Player.
**Why:** One Headless session; do not mix a windowed Player under a Headless Editor.
**Rejected:** Windowed editor spawning Headless Player in this slice.

### D5 — Dirty scene: last saved, no prompt
**Choice:** Headless Play uses the last saved Play entry (windowed "play last saved"). No GUI prompt, no auto-save. Live Capture still sees unsaved Op.
**Why:** No shell to prompt; Play Process world is the saved asset (same as v1 Play vs Capture).
**Rejected:** Auto-save; fail-closed `play.scene_dirty` (would block CI that forgot to save — caller can Op-save first).

### D6 — Same observation functions
**Choice:** `captureScene`, `PlaySessionController::step` / `requestPlayFrame`, Player `capturePlayProcessFrame`. Headless Player ticks offscreen then readback. `requestPlayFrame` must wait for the Player iterate (poll until frame or timeout) in tests; product wait loop is this slice for Headless IPC tests only.
**Why:** v1 risk: Headless must not invent a second API.
**Rejected:** Scene Thumbnail instantiate inside Headless Player; HWND BitBlt.

## Risks / Trade-offs

- [Vulkan device without surface on Windows] → Headless init path: instance + device, skip surface/swapchain; fail closed with a log if the GPU cannot create a device. Tests that need pixels stay GPU tests with the same PATH as other render tests.
- [SDL still inited for audio/time] → Video subsystem may be skipped; do not create a window.
- [PlaySessionController lives under UiHost today] → Mount it in `startSystems` for Headless Editor without UiHost.
- [Content Browser file watch without UI] → Keep it; no Slint sync.
- [requestPlayFrame one-shot poll] → Headless IPC tests wait on `poll()` until frame arrives (or timeout). Do not change windowed GUI this slice.

## Migration Plan

1. Launch flags + `headless` on global context; windowed default
2. Headless Editor boot (no window/Slint/UiHost; Authorship mounted)
3. Headless Vulkan/offscreen; Headless Player boot + Play frame readback
4. Play spawn `--headless`; IPC step/frame tests
5. Rollback: ignore `--headless`; always create the window as today

## Open Questions

None.
