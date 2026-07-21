# Task 3 Report — Play control channel (IPC)

**Status:** DONE  
**Branch:** `feat/play-mode-ui`  
**Workspace:** `E:/Dev/Blunder-Engine/.worktrees/play-mode-ui`

## Delivered (OpenSpec 3.1–3.3)

| Item | Result |
|------|--------|
| 3.1 Shared IPC helper | Done — `PlayIpcServer` / `PlayIpcClient` localhost TCP; ephemeral port via `listen(0)` |
| 3.2 Commands + ready | Done — line protocol `pause` / `resume` / `stop`; server emits `ready\n` on accept |
| 3.3 Loopback test | Done — `play_ipc_test` fake client against in-process server |
| Player wire | Done — `--play-ipc` starts server; poll each frame; Pause API + `requestClose` on stop |

OpenSpec 3.1–3.3 marked `[x]` in `openspec/changes/play-mode-ui/tasks.md`.

## Commits

1. `feat(play): localhost IPC control channel`

## TDD evidence

### RED

Stub `play_ipc.cpp` (no listen / no protocol):

```text
FAIL parse host:port ok
FAIL listen ephemeral
FAIL ready handshake
FAIL send pause
...
18 failure(s)
RED_EXIT=1
```

### GREEN

```text
play_ipc_test: all passed
GREEN_EXIT=0
```

## Protocol

- Endpoint: `host:port` (e.g. `127.0.0.1:54321`); port-only defaults host to `127.0.0.1`
- Server listens (non-blocking); on accept sends `ready\n`
- Client sends one command per line: `pause`, `resume`, `stop`
- Player: `pause`/`resume` → `setPlayPaused`; `stop` → `WindowSystem::requestClose`

## Self-review

- Unit test covers parse, ephemeral bind, ready handshake, and command dispatch without launching `engine_player`.
- Shared helper lives in `engine_runtime` for Task 4 editor client reuse.
- Player fails startup if `--play-ipc` is invalid or bind fails.

## Concerns

1. **Single client** — server accepts one editor connection; reconnect after drop is OK, multi-client not supported.
2. **No command ACKs** — editor must infer pause from Player state / UI; Task 4 may want acks.
3. **pipe: endpoints** — CLI still accepts non-TCP strings from Task 1 tests; Player rejects anything that is not `host:port`.
