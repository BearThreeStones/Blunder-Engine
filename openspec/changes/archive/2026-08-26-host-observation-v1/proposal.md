## Why

Authorship Query / Op / Diagnose can change and check a Project, but the agent still cannot **see** the result or **step** Play. Reads of the Live document are structured; Play IPC is only pause / resume / stop. Without Capture and Play frame, the loop is blind. Without Play step, “wait then screenshot” is not deterministic.

## What Changes

- Introduce **Host observation** beside the Authorship contract (not a fourth intent): **Capture**, **Play step**, **Play frame**
- **Capture** is a 16:9 **Scene still** (capped longest edge) via CPU readback: Play-rule camera, no Editor Overlays, same still path as Scene Thumbnail Render, does not write the Thumbnail cache. Live document uses the live SceneInstance; On-disk instantiates. No Camera is failure, not Editor Camera fallback
- **Play frame** is a still of the Play Process world (ticking or paused), not Capture, not a Scene still. Same 16:9 capped aspect. Rides the **Play control channel** (no second socket)
- **Play step** is legal only while **Play Pause**: N gameplay Ticks at fixed dt 1/60, stays paused, then Play frame can follow. Unpaused Play stays realtime
- Diagnostics stay **Console Messages**. No Host event bus. Play dump is out of this slice
- **Headless** is specified as the same observation without an OS window (not a third `EngineHostMode`); **this slice implements windowed Editor/Player**. Headless Editor (omit Slint) and Headless Player (offscreen Play frame) are a follow-up change
- ADR 0043 and CONTEXT terms already landed in grilling

**Out of scope:** CLI / MCP adapters; Headless host boot; Play dump; HWND / Slint chrome screenshots; Camera Preview as Capture source; main viewport / Gizmo Capture; Observe as Authorship intent; Query of the Player; a second Play socket; `BLUNDER_PLAYER_MAX_FRAMES` as Play step

## Capabilities

### New Capabilities
- `host-observation`: Capture (16:9 Scene still), Play step, Play frame; Host observation vs Authorship vs Console

### Modified Capabilities
- `play-mode`: Play Pause allows Play step at dt 1/60; Play control channel carries Play step and Play frame (not only pause / resume / stop)

## Impact

- Scene still path (Scene Thumbnail Render) gains an aspect parameter; square thumbs unchanged; Capture returns RGBA (or encoded still) to tests/API without writing thumbnail cache
- Play IPC: new commands / payloads for step and frame; `PlaySessionController` + Player `PlayIpcClient`
- Player: while paused, apply N fixed-dt ticks; readback Play-rule camera view for Play frame (windowed present path this slice)
- Tests: Capture (no camera fail, 16:9 not square, live dirty vs on-disk); Play step requires pause; Play frame after step
- Docs: `CONTEXT.md` (already); [ADR 0043](../../docs/adr/0043-host-observation.md)
