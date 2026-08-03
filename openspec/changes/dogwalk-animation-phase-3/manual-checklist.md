# Manual checklist — DogWalk animation Phase 3

Human-run validation for task **6.2**. Automated gates (`dogwalk_phase3_sync_cine_gate_test`, `dogwalk_phase3_mini_play_acceptance_test`, `animation_sync_group_test`, `cine_segment_service_test`, etc.) do **not** substitute for this pass.

**Status:** Not run — checklist authored with concrete paths. Check boxes when a human completes each step.

---

## Prerequisites

| Item | Value |
|------|--------|
| OpenSpec change | `openspec/changes/dogwalk-animation-phase-3/` |
| Engine worktree | `E:\Dev\Blunder-Engine\.worktrees\dogwalk-animation-phase-3` |
| Configure (once) | `cmake --preset vs2026-debug` (from worktree root) |
| Build (Debug) | `cmake --build build/vs2026-debug --config Debug --target engine_editor engine_player dogwalk_phase3_sync_cine_gate_test dogwalk_phase3_mini_play_acceptance_test` |
| Binaries | `build\vs2026-debug\bin\Debug\engine_editor.exe`, `engine_player.exe` (after full editor/player build) |
| Automated gates | `build\vs2026-debug\engine\src\tests\Debug\dogwalk_phase3_sync_cine_gate_test.exe`, `dogwalk_phase3_mini_play_acceptance_test.exe` |
| Test Project | `E:\Blunder Projects\Test` (or agreed harness project) |
| Entry scene | `E:\Blunder Projects\Test\Assets\Scenes\phase3_sync_cine.scene.asset` (Character + Partner; see `PHASE3_SYNC_CINE.md`) |
| Edit host policy | **Do not** set `BLUNDER_DOTNET_SCRIPTS=1` for Edit preview (Behaviour Tick must stay off) |

1. Open the Test Project in `engine_editor.exe` from this worktree build.
2. Load the Phase 3 mini acceptance scene (character + prop/partner): `Assets/Scenes/phase3_sync_cine.scene.asset`.
3. Confirm at least two Objects with co-located Skeleton + AnimationPlayer (`Character`, `Partner`).

**Run automated gates first (engineering bar):**

```powershell
cd E:\Dev\Blunder-Engine\.worktrees\dogwalk-animation-phase-3
cmake --build build/vs2026-debug --config Debug --target dogwalk_phase3_sync_cine_gate_test dogwalk_phase3_mini_play_acceptance_test
.\build\vs2026-debug\engine\src\tests\Debug\dogwalk_phase3_sync_cine_gate_test.exe
.\build\vs2026-debug\engine\src\tests\Debug\dogwalk_phase3_mini_play_acceptance_test.exe
```

---

## A. Edit Mode Sync Group + CINE marks (no Behaviour Tick)

- [ ] **A1 — Fire multi-Player**  
  In Edit Mode, Fire a Sync Group with per-member clip names (heterogeneous). Both Skeletons start aligned (hard cut).

- [ ] **A2 — in-CINE visible**  
  Enter CINE; confirm in-CINE (and suppression mark if exposed) is visible. End CINE; marks clear.

- [ ] **A3 — No Behaviour / no auto TRS**  
  During A1–A2, no Play session / no DotNetHost Behaviour Tick; Object positions do not auto-snap from CINE Enter/End alone.

**Pass:** Multi-Skeleton Fire + CINE marks work in Edit without gameplay scripts.

---

## B. Play mini SYNC + CINE acceptance

Engineering harness (`dogwalk_phase3_mini_play_acceptance_test`) covers synchronized start, CINE input suppression/restoration, and explicit End. **Human Play** still required for full product feel (Behaviours, viewport, real content).

- [ ] **B1 — Synchronized start**  
  Under Play, character + prop/partner start together via Sync Group (different clip names OK).

- [ ] **B2 — CINE handoff**  
  Enter CINE suppresses or hands off gameplay control as designed; End returns control to the player.

- [ ] **B3 — Explicit End**  
  Confirm ending the segment requires End (or script path that calls End); merely finishing one clip does not leave the game stuck in-CINE.

**Pass:** Mini DogWalk-style sync + handoff feels intentional; control returns cleanly.

---

## C. Regression / parallel gates

- [ ] **C1 — Phase 1/2 single-Player path**  
  Hard-cut and (if present) two-slot blend on a single character still work.

- [ ] **C2 — Phase 2 Chocomel weighted bar**  
  Phase 2 `tasks.md` **5.3** (Chocomel weighted idle↔walk Play acceptance) remains **open** — Phase 3 mini acceptance does **not** mark Phase 2 Done.

**Pass:** Phase 3 does not regress Phase 1/2 playback; Phase 2 content gate not silently closed.

---

## Sign-off

| Field | Value |
|-------|--------|
| Run by | |
| Date | |
| Build / commit | |
| Result | ☐ Pass &nbsp; ☐ Fail (note blockers below) |
| Notes | |

When all sections pass, a human may mark `tasks.md` **6.2** complete. This file does **not** auto-complete that task.
