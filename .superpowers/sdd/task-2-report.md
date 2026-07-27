# Task 2 Report — Gate OverlaySystem sync and draw

**Status:** DONE  
**Date:** 2026-07-27  
**Branch:** feat/player-hide-editor-overlays  
**OpenSpec:** player-hide-editor-overlays

## Objective

Wire `OverlaySystem` to `editorOverlaysEnabled(hostMode())` so Player host disables authorship overlay sync and all draw paths (grid, gizmos, outline, AA, screen pass).

## Files modified

| Path | Change |
|------|--------|
| `engine/src/runtime/function/render/overlay/overlay_system.h` | Added private `authorshipOverlaysActive()` and `disableAuthorshipOverlays()` |
| `engine/src/runtime/function/render/overlay/overlay_system.cpp` | Include policy header; gate `begin_sync` + all `draw_*` entry points |
| `openspec/changes/player-hide-editor-overlays/tasks.md` | Marked 2.1 and 2.2 complete |

## Interfaces delivered

**Consumes:** `editorOverlaysEnabled(g_runtime_global_context.hostMode())` from `editor_overlay_policy.h`

**Produces:** When Player host:
- `begin_sync` clears selection, sets all authorship `enabled_ = false`, returns before per-overlay `begin_sync`
- `draw_scene_overlays`, `draw_outline`, `draw_overlay_lines`, `draw_overlay_aa`, `draw_screen_overlays` early-return (screen pass not begun)

## Implementation notes

- `disableAuthorshipOverlays()` sets `enabled_` directly on grid, axes, wireframe, origins, outline, navigate_gizmo, transform_gizmo, anti_aliasing — matches existing overlay style (public `enabled_` on `Overlay` base).
- Pick/hybrid-pick paths untouched — not authorship chrome per spec.
- Task 3 (RenderSystem input gate) intentionally not implemented.

## Build evidence

**Command:**
```powershell
cmake --build build/vs2026-debug --config Debug --target engine_player
```

**Result:** Exit code 0.

**Link (excerpt):**
```
engine_player.vcxproj -> E:\Dev\Blunder-Engine\build\vs2026-debug\bin\Debug\engine_player.exe
```

## Smoke evidence

**Command:**
```powershell
$env:BLUNDER_PLAYER_MAX_FRAMES='90'
& .\build\vs2026-debug\bin\Debug\engine_player.exe `
  --project-root "E:\Blunder Projects\Test" `
  --scene "assets/Scenes/pick_test.scene.asset"
```

**Result:** Exit code 0.

**Log highlights:**
- `Player host mode — skipping Slint editor shell`
- Scene `pick_test.scene.asset` loaded (5 entities, 3 cube meshes)
- Vulkan/renderer init + 90 frames completed without error

**Visual:** Headless/CI agent cannot confirm viewport pixels; code gates all authorship draw paths and `draw_screen_overlays` returns before `m_screen_pass.begin(cmd)`. Manual visual check recommended: scene meshes visible, no ground grid / Transform / Navigate gizmo.

## Verification checklist

| Step | Status |
|------|--------|
| `authorshipOverlaysActive()` + `disableAuthorshipOverlays()` | Done |
| Gate `begin_sync` (early return in Player) | Done |
| Gate `draw_scene_overlays` | Done |
| Gate `draw_outline` | Done |
| Gate `draw_overlay_lines` | Done |
| Gate `draw_overlay_aa` | Done |
| Gate `draw_screen_overlays` (before screen pass begin) | Done |
| `engine_player` build | Pass |
| Player smoke run | Pass (exit 0) |
| OpenSpec tasks 2.1 / 2.2 | Marked complete |

## Not done (per brief)

- Task 3: RenderSystem event input gate
- New Vulkan/integration tests (policy covered by Task 1 unit test)

## Concerns

- Gizmo/grid shaders still compile at init in Player (shader assets loaded during `OverlaySystem::initialize`); only sync/draw gated — acceptable per design, no runtime cost when `enabled_ == false`.
- Visual overlay absence not pixel-verified in this run; rely on code review + manual QA.
- `hasActiveLineOverlays()` / `hasActiveOutline()` reflect disabled state after `begin_sync` in Player — downstream forward path should skip overlay passes; worth confirming no empty pass overhead in a follow-up perf check.
