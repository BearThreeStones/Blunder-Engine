# Task 2 Report — Player host + Pause

**Status:** DONE  
**Branch:** `feat/play-mode-ui`  
**Workspace:** `E:/Dev/Blunder-Engine/.worktrees/play-mode-ui`

## Delivered (OpenSpec 2.1–2.4)

| Item | Result |
|------|--------|
| Prerequisite: Player Vulkan AV | Fixed — `PickOverlay` descriptor pool includes standalone `SAMPLER` (binding 2); allocate/update no longer hit `VK_NULL_HANDLE` |
| 2.1 DotNetHost in Player | Done — Player `force_start`; stage ScriptHost/Api beside `engine_player`; load `.blunder/scripts_bin` game DLL when present |
| 2.2 Mount after scene load | Done — `mountSceneBehaviours` after Player entry scene load when host running |
| 2.3 Pause tick gate | Done — `shouldAdvanceBehaviourTick` / `dispatchObjectLifecycle`; `RuntimeGlobalContext::{is,set}PlayPaused`; engine tick uses gate |
| 2.4 Window close exit | Done — `tickOneFrame` returns false on `shouldClose`; SDL quit path; smoke exit 0 (also `BLUNDER_PLAYER_MAX_FRAMES`) |

OpenSpec checkboxes 2.1–2.4 marked in `openspec/changes/play-mode-ui/tasks.md`.

## TDD evidence

### RED

Stub `shouldAdvanceBehaviourTick` always returned `true`.

```text
FAIL gate blocks when paused
FAIL tick skipped while paused
FAIL tick resumes after pause
3 failure(s)
RED_EXIT=1
```

### GREEN

```text
play_pause_tick_gate_test: all passed
GREEN_EXIT=0
```

## Player smoke (AV + host + exit)

```text
[info] [DotNetHost] ScriptHost running
[info] [DotNetHost] no game DLL under .blunder/scripts_bin (host only)
[info] engine start
[info] [BlunderEngine] Player smoke exit after 30 frames
EXIT=0x00000000
```

Also: process stayed alive ≥8s; `taskkill` (WM_CLOSE) → exit 0. No `dstSet is VK_NULL_HANDLE` / `0xC0000005`.

## Commits

1. `fix(player): avoid Vulkan overlay AV in Player host mode`
2. `feat(player): DotNetHost mount and Pause tick gate`

## Concerns

1. No project game DLL under `.blunder/scripts_bin` in this worktree — host-only path verified; mount with real Behaviours needs a built Scripts assembly.
2. Player still constructs editor subsystems (content browser, scene-edit); thumbnail/content teardown order fixed so VMA does not assert on Player exit.
3. `BLUNDER_PLAYER_MAX_FRAMES` is a smoke helper, not product UI Pause.
4. Mesh import for `pick_test` still fails on `.dae` in this worktree (pre-existing asset path issue); does not block loop/host.
