# Task 2 Report — Player host + Pause

**Status:** DONE  
**Branch:** `feat/play-mode-ui`  
**Workspace:** `E:/Dev/Blunder-Engine/.worktrees/play-mode-ui`

## Delivered (OpenSpec 2.1–2.4)

| Item | Result |
|------|--------|
| Prerequisite: Player Vulkan AV | Fixed — `PickOverlay` descriptor pool includes standalone `SAMPLER` (binding 2); allocate no longer returns null → `vkUpdateDescriptorSets` AV |
| 2.1 DotNetHost in Player | Done — Player `force_start`; stage ScriptHost/Api beside `engine_player`; load `.blunder/scripts_bin` game DLL when present |
| 2.2 Mount after scene load | Done — `mountSceneBehaviours` after Player entry scene load when host running (`loadScene` also mounts) |
| 2.3 Pause tick gate | Done — `play_tick_gate.h` + `RuntimeGlobalContext::{is,set}PlayPaused`; engine uses `dispatchObjectLifecycle` |
| 2.4 Window close exit | Done — `shouldClose` / SDL quit; WM_CLOSE → shutdown; also `BLUNDER_PLAYER_MAX_FRAMES` smoke helper |

OpenSpec 2.1–2.4 marked `[x]` in `openspec/changes/play-mode-ui/tasks.md`.

## Commits

1. `71992bd` — `fix(player): avoid Vulkan overlay AV in Player host mode`
2. `2d8e4e9` — `feat(player): DotNetHost mount and Pause tick gate`
3. `9c01828` — report-only follow-up (same feat message)

## TDD evidence

### RED

Stub `shouldAdvanceBehaviourTick` always returned `true` (`.superpowers/sdd/task-2-red-run.txt`):

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

## Player smoke

```text
[info] [DotNetHost] ScriptHost running
[info] [DotNetHost] no game DLL under .blunder/scripts_bin (host only)
[info] [RuntimeGlobalContext] Player host mode — skipping Slint editor shell
[info] engine start
title=Blunder - 1210 FPS
[info] [Event] Window close requested
[info] engine shutdown
```

Also: `BLUNDER_PLAYER_MAX_FRAMES=30` → `Player smoke exit after 30 frames`. No `dstSet is VK_NULL_HANDLE` / `0xC0000005`.

## Self-review

- Pause gate is unit-tested; engine wires the same helper.
- Player always starts DotNetHost; editor remains env-gated.
- ScriptHost staging beside `engine_player` is required for 2.1 (exe-dir load).
- AV root cause was PickOverlay pool type mismatch (confirmed under `BLUNDER_VK_VALIDATION=1` pre-fix).

## Concerns

1. **VMA assert on shutdown** — Debug still prints `m_pMetadata->IsEmpty()` after allocator destroy despite moving content-browser/thumbnail teardown before render; process still reaches shutdown / leaves the loop (not `0xC0000005`).
2. **No game DLL** under `.blunder/scripts_bin` in this worktree — host-only path verified; Behaviour mount with a real assembly needs Scripts build.
3. **`pick_test` mesh import** — early load can fail opening `.dae` via glTF path; loop/host still run.
4. **`BLUNDER_PLAYER_MAX_FRAMES`** — smoke helper only, not product Pause UI (IPC is Task 3).
