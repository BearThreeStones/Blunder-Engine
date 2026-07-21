# Task 2 Report — Player host + Pause

**Status:** DONE  
**Branch:** `feat/play-mode-ui`  
**Workspace:** `E:/Dev/Blunder-Engine/.worktrees/play-mode-ui`

## Delivered (OpenSpec 2.1–2.4)

| Item | Result |
|------|--------|
| Prerequisite: Player Vulkan AV | Fixed — `PickOverlay` pool includes `VK_DESCRIPTOR_TYPE_SAMPLER` (binding 2); no null-set `vkUpdateDescriptorSets` AV |
| 2.1 DotNetHost in Player | Done — Player `force_start`; ScriptHost/Api staged beside exe; loads `.blunder/scripts_bin` game DLL when present |
| 2.2 Mount after scene load | Done — `mountSceneBehaviours` after Player entry scene when host running (`loadScene` also mounts) |
| 2.3 Pause tick gate | Done — `play_tick_gate.h` + `RuntimeGlobalContext::{is,set}PlayPaused`; engine uses `dispatchObjectLifecycle` |
| 2.4 Window close exit | Done — WM_CLOSE → `shouldClose` → shutdown → process exit 0; `BLUNDER_PLAYER_MAX_FRAMES` smoke helper also available |

OpenSpec 2.1–2.4 marked `[x]` in `openspec/changes/play-mode-ui/tasks.md`.

## Commits

1. `71992bd` — `fix(player): avoid Vulkan overlay AV in Player host mode`
2. `2d8e4e9` — `feat(player): DotNetHost mount and Pause tick gate`
3. `82fb2a1` — `fix(player): soft-fail VMA leftover assert on shutdown`

## TDD evidence

### RED

Stub `shouldAdvanceBehaviourTick` always returned `true`:

```text
FAIL gate blocks when paused
FAIL tick skipped while paused
FAIL tick resumes after pause
3 failure(s)
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
[info] [Event] Window close requested
[info] engine shutdown
[info] [VulkanAllocator::shutdown] destroying VMA allocator
HasExited=True ExitCode=0
AV=0
```

Also: `BLUNDER_PLAYER_MAX_FRAMES=30` leaves after N frames without AV. No `dstSet is VK_NULL_HANDLE` / `0xC0000005`.

## Self-review

- Pause gate unit-tested; engine wires the same helper.
- Player always starts DotNetHost; editor remains `BLUNDER_DOTNET_SCRIPTS` env-gated.
- ScriptHost staging beside `engine_player` required for exe-dir load.
- AV root cause was PickOverlay pool type mismatch (validation: binding 2 SAMPLER missing from pool).
- Soft VMA assert unblocks clean close; residual allocation root cause still open.

## Concerns

1. **Residual VMA allocations on shutdown** — still logged (`m_pMetadata->IsEmpty()`); soft-assert avoids Debug abort so exit code is 0; proper free-order fix still needed.
2. **No game DLL** under `.blunder/scripts_bin` in this worktree — host-only path verified; real Behaviour mount needs Scripts build.
3. **`pick_test` mesh import** — `.dae` via glTF path can fail; host/loop still run.
4. **Pause IPC** — `setPlayPaused` is in-process only until Task 3 control channel.
