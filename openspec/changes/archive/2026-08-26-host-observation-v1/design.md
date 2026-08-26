## Context

See proposal.md. Grilling locked CONTEXT and [ADR 0043](../../../docs/adr/0043-host-observation.md). **Scene Thumbnail Render** already: instantiate On-disk scene (or equivalent), `resolvePlayCameraFromScene`, dedicated offscreen, no overlays, CPU readback, square, cache write (`SceneThumbnailRenderService`). Live Capture must use the live `SceneInstance` instead of disk instantiate. **Play control channel** is line-oriented loopback IPC: pause / resume / stop plus NDJSON log records (`PlayIpcServer` / `PlayIpcClient`). Player Pause sets `g_runtime_global_context.setPlayPaused`. Player ticks `tickOneFrame(calculateDeltaTime())` — variable dt. `BLUNDER_PLAYER_MAX_FRAMES` exits the process. This slice is **windowed** Editor/Player; Headless is a follow-up on the same contract.

## Goals / Non-Goals

**Goals:**
- C++ Capture API tests can call without MCP/CLI (may use existing render test harness / Scene Thumbnail service)
- Shared Scene still path with aspect parameter (square thumb vs 16:9 Capture)
- Play IPC step + frame; Player honors step only when paused, fixed dt 1/60
- Windowed Play frame from the Player's game view (Play-rule camera), not HWND scrape

**Non-Goals:**
- Headless Editor (omit Slint) / Headless Player offscreen-only present
- Play dump JSON
- PNG as the only in-memory test format (wire may encode; tests may see w/h + pixels)
- Authorship System methods for Capture (wrong contract)
- Changing Scene Thumbnail lighting / overlay / camera resolve rules

## Decisions

### D1 — Capture is not Authorship System
**Choice:** Capture lives on the Scene still / thumbnail render service (or a thin wrapper beside it), not `AuthorshipSystem`. Request failure `capture.no_camera` (and `capture.no_live_document` / `capture.scene_unreadable` mirroring authorship subjects).
**Why:** ADR 0043: Host observation beside Authorship.
**Rejected:** `AuthorshipSystem::capture`; Diagnose returning PNG.

### D2 — Aspect parameter on the still path
**Choice:** Scene still request carries aspect (and longest-edge cap). Thumbs keep square. Capture uses 16:9, longest edge **1280** (720p). Same camera resolve, overlays off, lighting as today's Scene Thumbnail Render.
**Why:** Grilling C; 480 (Camera Preview PiP) is too small for agents; uncapped wastes IPC.
**Rejected:** Square Capture; live window aspect; 480 longest edge.

### D3 — Live vs On-disk still source
**Choice:** Live: draw the active `SceneInstance` (no disk re-instantiate). On-disk: existing `renderSceneAsset` instantiate path. Neither writes thumbnail cache when `write_cache` is false (Capture).
**Why:** Unsaved Op must show; thumbs stay last-saved.
**Rejected:** Capture always instantiate from disk; Capture always dirty live for On-disk subject.

### D4 — Play IPC text lines, one socket
**Choice:** Editor sends `step <n>` and `frame` (or equivalent parseable lines). Player replies with an NDJSON frame record (width, height, encoding, payload) on the same connection as logs. Playing-state `step` is ignored/failed (`play.step_requires_pause`); do not tick.
**Why:** Channel is already line/NDJSON; Avoid second socket.
**Rejected:** Binary sidecar socket; HTTP; embedding frames in Console Messages.

### D5 — Fixed dt only for Play step
**Choice:** While paused, `step N` calls the engine tick N times with `dt = 1.0/60.0`. Unpaused `tickOneFrame` still uses `calculateDeltaTime()`.
**Why:** Grilling C; human Play stays realtime.
**Rejected:** Variable dt step; lockstep entire Play session; `MAX_FRAMES` process exit.

### D6 — Windowed Play frame readback
**Choice:** After the last step tick (or on `frame` while paused), CPU-readback the Player color target that already shows the Play-rule camera. Do **not** run Scene Thumbnail instantiate inside the Player (that path is bind/rest, On-disk).
**Why:** Play frame is the ticking world. Windowed slice can use the existing presentable target.
**Rejected:** HWND BitBlt; Scene still inside `engine_player`; requiring Headless offscreen in this slice.

## Risks / Trade-offs

- [Scene Thumbnail service is editor-only / GPU] → Capture tests that need a real still may need the same GPU PATH as other render tests; add a no-GPU unit for failure codes (no camera, subject) without pixels.
- [Huge NDJSON frame lines] → Cap 1280; consider PNG encoding on the wire if raw RGBA is too large.
- [Pause flag vs Starting state] → Step only in Paused; Starting/Stopped fail closed.
- [Player readback missing today] → New code on Player render path; keep editor viewport readback untouched.
- [Headless deferred] → Windowed tests prove the contract; Headless must not invent a second API later.

## Migration Plan

1. Scene still aspect + Capture API + tests (fail codes, 16:9, cache not written)
2. Play IPC `step` / `frame` parse + `play_ipc_test`
3. Player: paused fixed-dt step + color readback; editor session hooks
4. `play_session_controller_test` (fake hooks) for step-requires-pause and frame after step
5. Rollback: revert still-parameter default square; revert IPC verbs; Player ignores unknown lines as today

## Open Questions

None.
