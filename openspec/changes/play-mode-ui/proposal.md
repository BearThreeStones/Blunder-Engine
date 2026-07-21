## Why

Gameplay Scripts and Behaviour scene serialization exist, but authors still cannot enter **Play Mode** from the editor: DotNetHost is stuck behind `BLUNDER_DOTNET_SCRIPTS`, and there is no Play/Pause/Stop session. DogWalk and day-to-day scripting need a first-class try-the-game path. ADR 0014 locks Play to a **separate Player process**.

## What Changes

- Add dedicated **Player** executable `engine_player` (thin main over shared `engine_runtime`) that loads a Project, the Play entry scene (saved asset), starts DotNetHost, mounts Behaviours, and runs gameplay Tick.
- Wire editor **Play / Pause / Stop** controls (existing Play menu stub) to a **Play session**: at most one Player; local **IPC** for pause / resume / stop; Player window close = Stop.
- On Play: dirty-scene prompt (Save and Play / Play last saved / Cancel); **Scripts build when dirty** before spawn; pass project root + entry scene to Player.
- Edit Mode **does not** start DotNetHost for normal authorship; env-gated editor host remains debug/test only.
- Docs: testing.md / CONTEXT / ADR 0014 already decided; keep agent guides in sync.
- **Out of this change:** live sync of edits/assets/ALC into a running Player; Pause that freezes rendering only; multi-Player; packaging a shipping game binary beyond the editor-adjacent Player.

## Capabilities

### New Capabilities

- `play-mode`: Editor-side Play session — controls, dirty prompt, Scripts-dirty build, spawn/track single Player, UI state (playing / paused / stopped).
- `play-player`: Player process — load Project + entry scene, host Scripts, mount Behaviours, Pause skips Behaviour Tick while window stays viewable, exit ends session.
- `play-control-channel`: Local IPC between editor and Player for pause, resume, and stop (plus ready/heartbeat as needed).

### Modified Capabilities

- `dotnet-script-host`: Product Play path starts the host inside the Player; Edit Mode does not require an editor-process host.
- `behaviour-scene`: Mount-on-load applies in the Player when Play starts (same restore/mount rules; host now lives in Player).

## Impact

- New target: `engine_player` (+ CMake / bin staging beside editor)
- Editor UI: Play/Pause/Stop callbacks; dirty dialog; session controller
- Spawn/relaunch helpers (sibling of `project_relaunch`)
- IPC server/client (named pipe or localhost — design picks)
- `tryStartDotNetHost` / `BLUNDER_DOTNET_SCRIPTS` demoted for product Play
- Player main: project root, scene path, always-on host for session
- Tests: session controller fakes; IPC smoke; Player CLI smoke where feasible
- Docs: `docs/agents/testing.md`, build notes for `engine_player`
