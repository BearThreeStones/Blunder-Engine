# Task 3 Report — Gate Transform / Navigate interaction in RenderSystem

**Status:** DONE  
**Date:** 2026-07-27  
**Branch:** feat/player-hide-editor-overlays  
**OpenSpec:** player-hide-editor-overlays

## Objective

Gate `RenderSystem::onEvent` Transform/Navigate gizmo input paths with `editorOverlaysEnabled(hostMode())` so the Player host never marks events handled via editor overlay gizmos. RenderDoc F11 and Editor Camera orbit / viewport pick remain ungated.

## Files modified

| Path | Change |
|------|--------|
| `engine/src/runtime/function/render/render_system.cpp` | Include `editor_overlay_policy.h`; wrap three gizmo/nav event blocks with `overlays` flag |
| `openspec/changes/player-hide-editor-overlays/tasks.md` | Marked 3.1 complete; 3.2 USER-VERIFY |

## Interfaces delivered

**Consumes:** `editorOverlaysEnabled(g_runtime_global_context.hostMode())` from `editor_overlay_policy.h`

**Produces:** When Player host:
- `transform_gizmo().controller().onEvent` skipped
- Transform + Navigate mouse-move hover updates skipped
- Navigate gizmo left-click handling skipped
- `m_editor_camera->onEvent` (orbit) still runs
- RenderDoc F11 capture still runs

## Implementation notes

- Single `const bool overlays = editorOverlaysEnabled(...)` at top of gizmo section; three `if (overlays && ...)` blocks per brief.
- Viewport pick, piercing menu, and `m_editor_camera->onEvent` paths unchanged (after gizmo blocks).

## Build evidence

**Command:**
```powershell
cmake --build build/vs2026-debug --config Debug --target engine_player engine_editor
```

**Result:** Exit code 0.

## Test evidence

**Policy unit test (direct run):**
```powershell
cmake --build build/vs2026-debug --config Debug --target editor_overlay_policy_test
.\build\vs2026-debug\engine\src\tests\Debug\editor_overlay_policy_test.exe
```

**Result:** `editor_overlay_policy_test: all passed`

**Note:** `ctest -R editor_overlay_policy_test` reported "No tests were found" in this build tree (CTest discovery gap); binary run confirms pass.

**Player smoke:**
```powershell
$env:BLUNDER_PLAYER_MAX_FRAMES='90'
.\build\vs2026-debug\bin\Debug\engine_player.exe `
  --project-root "E:\Blunder Projects\Test" `
  --scene "assets/Scenes/pick_test.scene.asset"
```

**Result:** Exit code 0; 90 frames completed.

## USER-VERIFY checklist (3.2 — dual-window GUI)

Agent cannot run interactive dual-window QA. Manual verification:

1. Launch `engine_editor` with Test project.
2. Editor viewport: grid + Navigate gizmo visible; Transform gizmo works on selection.
3. Press **Play**; focus Player window — no grid / Transform / Navigate overlays.
4. Click/drag in Player viewport — gizmo does not capture input; camera orbit still works (product bindings).
5. **Pause** from editor — Player overlays still hidden.
6. **Stop** — editor overlays unchanged.

## Verification checklist

| Step | Status |
|------|--------|
| Include `editor_overlay_policy.h` | Done |
| Gate transform `controller().onEvent` | Done |
| Gate mouse-move hover (transform + nav) | Done |
| Gate navigate left-click | Done |
| Do not gate RenderDoc F11 | Done |
| Do not gate Editor Camera orbit | Done |
| `engine_player` + `engine_editor` build | Pass |
| `editor_overlay_policy_test` | Pass (direct) |
| Player smoke run | Pass |
| OpenSpec 3.1 | Marked complete |
| OpenSpec 3.2 dual-window | USER-VERIFY |

## Concerns

- CTest does not discover `editor_overlay_policy_test` in `build/vs2026-debug`; run binary directly or fix CTest registration in a follow-up.
- Gizmo input absence in Player not interactively verified; relies on code gate + Task 2 draw gate + smoke run.
- Player smoke still logs VMA allocation warning on shutdown (pre-existing, unrelated).
