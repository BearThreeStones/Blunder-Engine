# Task 4 Report — Editor Play session

**Status:** DONE  
**Branch:** `feat/play-mode-ui`  
**Workspace:** `E:/Dev/Blunder-Engine/.worktrees/play-mode-ui`

## Delivered (OpenSpec 4.1–4.4)

| Item | Result |
|------|--------|
| 4.1 `PlaySessionController` | Done — states Stopped / Starting / Playing / Paused; injectable hooks for process + IPC |
| 4.2 Spawn `engine_player` | Done — argv builder + sibling/staged resolve; `--project-root`, `--scene`, `--play-ipc` |
| 4.3 Single-session + exit | Done — Stop before new Play; `poll()` maps process exit → Stopped |
| 4.4 Slint Play/Pause/Stop | Done — wired callbacks + Pause enabled only after IPC `ready` |

## Commits

1. `dbde849` — `feat(editor): PlaySessionController and Play/Pause/Stop UI`

## TDD evidence

### RED (stub controller)

```text
FAIL argv size
...
RED_EXIT != 0
```

Logged: `.superpowers/sdd/task-4-red-build.txt`, `task-4-red-run.txt`

### GREEN

```text
play_session_controller_test: all passed
EXIT=0
```

Logged: `.superpowers/sdd/task-4-green-build.txt`, `task-4-green-run.txt`

Also rebuilt `engine_runtime` (Slint regen + UI wiring) successfully.

## Design notes

- Hooks allow fake process/IPC in unit tests without launching Player.
- Production hooks: ephemeral loopback port probe, `CreateProcessW` / `posix_spawn`, real `PlayIpcClient`.
- Resolve path: sibling `engine_player` first; fallback to CMake `.../player/<Config>/` when editor lives under `.../editor/<Config>/`.
- UI: Pause button label flips to Resume while paused; Pause disabled until ready.

## Self-review

- State machine covered by fake-hook tests (start/ready/pause/resume/stop/exit/single-session/early-pause).
- Editor owns one `PlaySessionController` on `RuntimeGlobalContext` (Editor host only).
- Out of scope left for Task 5: dirty prompt, Scripts build gate, spawn-error toast.

## Concerns

1. **Ephemeral port race** — editor binds port 0 then closes before Player listens; rare collision possible.
2. **Hard terminate** — Stop sends IPC then `TerminateProcess` without waiting for graceful exit.
3. **Spawn failures silent in UI** — `lastError()` is set but not surfaced in Slint yet.
4. **No end-to-end editor→Player smoke** in this task (unit + runtime build only).
