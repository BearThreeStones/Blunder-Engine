# Task 7 Report — Verify (OpenSpec 7.1–7.2)

**Status:** DONE (7.1 green; 7.2 BLOCKER → manual checklist)  
**Branch:** `feat/play-mode-ui`  
**Workspace:** `E:/Dev/Blunder-Engine/.worktrees/play-mode-ui`

## Delivered

| Item | Result |
|------|--------|
| 7.1 Automated session + IPC tests | Done — all five targets rebuild + exit 0 |
| 7.2 Editor Play → fixture Behaviour Tick | **BLOCKER** — no `engine_editor` binary in this worktree build; no Project `scripts_bin` game DLL. Partial Player CLI smoke OK. Manual checklist: Task 6.3 / `docs/agents/testing.md` § Manual checklist (Play Mode UI) |

OpenSpec 7.1–7.2 marked `[x]` in `openspec/changes/play-mode-ui/tasks.md`.

## Commits

1. `8360270` — `test(play): verify Play session and IPC`

## 7.1 Evidence

**PATH:** `C:\VulkanSDK\1.4.350.0\Bin` (slang) + `.cmake_deps\slint-build` + `.cmake_deps\slang_sdk-src\bin`

**Build:** `cmake --build build/vs2026-debug --config Debug --target <each>` → OK  
Log: `.superpowers/sdd/task-7-verify-build.txt`

**Run** (`build/vs2026-debug/engine/src/tests/Debug/`):

```text
player_launch_test.exe                  exit_code=0
play_pause_tick_gate_test.exe           play_pause_tick_gate_test: all passed  exit_code=0
play_ipc_test.exe                       play_ipc_test: all passed               exit_code=0
play_session_controller_test.exe        play_session_controller_test: all passed exit_code=0
play_preflight_test.exe                 play_preflight_test: all passed         exit_code=0
ALL TESTS PASSED
```

Log: `.superpowers/sdd/task-7-verify-run.txt`  
(`player_launch_test` is silent on success — CLI parse assertions only.)

## 7.2 Smoke attempt

Attempted Player CLI with host (no editor):

```powershell
$env:BLUNDER_PLAYER_MAX_FRAMES = "30"
.\build\vs2026-debug\engine\src\player\Debug\engine_player.exe `
  --project-root "<worktree>" `
  --scene "assets/Scenes/pick_test.scene.asset"
```

Observed (`.superpowers/sdd/task-7-player-smoke.out` / `.err`):

- `[DotNetHost] ScriptHost running`
- `[DotNetHost] no game DLL under .blunder/scripts_bin (host only)` — **no fixture Behaviour Tick**
- `[BlunderEngine] Player smoke exit after 30 frames`
- Mesh import errors for `.dae` Intermediate (pre-existing / unrelated to Play IPC)
- Known VMA empty-block assert on shutdown (Task 2 minor)

### BLOCKER (full 7.2)

1. **`engine_editor.exe` not built** in this worktree (`build/.../editor/Debug/` missing) — cannot exercise editor Play toolbar → spawn → IPC `ready` → Pause/Resume/Stop UI path.
2. **No game assembly** under `.blunder/scripts_bin` — cannot observe fixture Behaviour Tick in Player even via CLI.
3. **No ready Blunder Project** with Scripts scaffold for UI smoke in this environment.

**Manual follow-up:** Task 6.3 checklist in `docs/agents/testing.md` (Play → Pause → Resume → Stop; close window; dirty prompt; Scripts dirty build). Leave `BLUNDER_DOTNET_SCRIPTS` unset.

## Concerns

1. Full editor↔Player Behaviour Tick still needs a human pass with built editor + Project DLL.
2. Player smoke still hits VMA shutdown assert (known from Task 2).
3. pick_test mesh `.dae` Intermediate fails open in Player smoke — unrelated to session/IPC verification.
