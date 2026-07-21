### Task 1: Player executable skeleton (OpenSpec 1.1–1.4)

**Plan:** `docs/superpowers/plans/2026-07-21-play-mode-ui.md`  
**OpenSpec tasks:** 1.1–1.4  
**Workspace:** `E:/Dev/Blunder-Engine/.worktrees/play-mode-ui`

**Requirements (verbatim from plan):**

Add `engine_player` CMake target (thin main, link `engine_runtime` / needed deps; stage beside `engine_editor`).

Parse CLI: `--project-root`, `--scene`, `--play-ipc` (endpoint).

Boot Player without editor Slint shell: window + engine loop + FileSystem project root.

Load Play entry scene asset and confirm manual run from command line.

**TDD (mandatory):**
1. RED: CLI parse unit test for `--project-root` / `--scene` / `--play-ipc` fails (missing feature).
2. GREEN: parse helpers + `engine_player` target; boot path loads project + scene (minimal).
3. Commit: `feat(player): add engine_player skeleton and CLI`

Mark OpenSpec checkboxes 1.1–1.4 in `openspec/changes/play-mode-ui/tasks.md`.

**Patterns to follow:**
- Editor: `engine/src/editor/` thin main + CMake linking `engine_runtime`
- Launch parse: `runtime/project/editor_launch.h` style (testable without Vulkan if possible)
- Prefer extractable `PlayerLaunch` / CLI parse in runtime or player lib testable by `player_launch_test`

**Out of scope for Task 1:** DotNetHost, Pause, IPC server, editor UI.

**Global constraints:** TDD; ADR 0014 separate Player; no unrelated WIP; model commits scoped.
