# Manual checklist — DogWalk animation Phase 4

Human-run validation for task **6.2** (Chocomel-subset Play acceptance). Automated gates (`dogwalk_phase4_tree_gate_test`, `dogwalk_phase4_mini_play_acceptance_test`, `animation_tree_test`, `animation_preview_controller_test`, `animation_sync_group_test`, etc.) do **not** substitute for this pass.

**Status:** Not run — checklist authored with concrete paths. Check boxes when a human completes each step.

---

## Prerequisites

| Item | Value |
|------|--------|
| OpenSpec change | `openspec/changes/dogwalk-animation-phase-4/` |
| Engine worktree | `E:\Dev\Blunder-Engine\.worktrees\dogwalk-animation-phase-4` |
| Configure (once) | `cmake --preset vs2026-debug` (from worktree root) |
| Build (Debug) | `cmake --build build/vs2026-debug --config Debug --target engine_editor engine_player dogwalk_phase4_tree_gate_test dogwalk_phase4_mini_play_acceptance_test` |
| Binaries | `build\vs2026-debug\bin\Debug\engine_editor.exe`, `engine_player.exe` |
| Automated gates | `build\vs2026-debug\engine\src\tests\Debug\dogwalk_phase4_tree_gate_test.exe`, `dogwalk_phase4_mini_play_acceptance_test.exe` |
| Test Project | `E:\Blunder Projects\Test` (or agreed harness project) |
| Chocomel subset | Character with scene-embedded `animationTree` (BlendSpace1D locomotion, Add2 turn, OneShot trip/SYNC clips) |
| Edit host policy | **Do not** set `BLUNDER_DOTNET_SCRIPTS=1` for Edit preview (Behaviour Tick must stay off) |

1. Open the Test Project in `engine_editor.exe` from this worktree build.
2. Load a scene with Chocomel (or agreed subset) using an active scene-embedded AnimationTree.
3. Confirm co-located Skeleton + AnimationPlayer + AnimationTree on the character Object.

**Run automated gates first (engineering bar):**

```powershell
cd E:\Dev\Blunder-Engine\.worktrees\dogwalk-animation-phase-4
cmake --build build/vs2026-debug --config Debug --target dogwalk_phase4_tree_gate_test dogwalk_phase4_mini_play_acceptance_test
.\build\vs2026-debug\engine\src\tests\Debug\dogwalk_phase4_tree_gate_test.exe
.\build\vs2026-debug\engine\src\tests\Debug\dogwalk_phase4_mini_play_acceptance_test.exe
```

---

## A. Edit Mode AnimationTree scrub (no Behaviour Tick)

- [ ] **A1 — Activate tree + Travel**  
  In Edit Mode, activate AnimationTree and Travel/Start to Locomotion. Skeleton updates without Play session.

- [ ] **A2 — BlendSpace scalar scrub**  
  Scrub BlendSpace1D scalar (speed-like). Perceptible locomotion blend change on the character.

- [ ] **A3 — OneShot + Add2 scrub**  
  Request OneShot (trip-like clip) and scrub Add2 weight/clip. OneShot returns to base; Add2 overlay visible at non-zero weight.

- [ ] **A4 — TimeScale scrub**  
  Scrub AnimationPlayer TimeScale while tree is active; preview advances faster/slower. No Behaviour Tick during A1–A4.

**Pass:** Edit tree scrub works without DotNetHost / Behaviour Tick and without a visual graph editor.

---

## B. Play Chocomel-subset acceptance

Engineering harness (`dogwalk_phase4_mini_play_acceptance_test`) covers Play-path BlendSpace, Add2, OneShot return, dominant base clock, and Sync Fire→OneShot. **Human Play** still required for full product feel (Behaviours, viewport, real Chocomel content).

- [ ] **B1 — BlendSpace locomotion**  
  Under Play, speed-like BlendSpace motion is perceptible (idle↔walk or equivalent subset clips).

- [ ] **B2 — Visible additive turn**  
  Add2 turn/bark overlay is visible on locomotion base (exact Godot clip names not required).

- [ ] **B3 — OneShot return-to-base**  
  Trip/interrupt OneShot plays then returns to locomotion base without deactivating the tree.

- [ ] **B4 — Stepped facing on base clock**  
  Stepped facing still follows the **base dominant clip** clock (not Add2); real-time move remains Tick-driven.

**Pass:** Chocomel-subset Play feels intentional; tree stays active; step sync uses base clock.

---

## C. Regression / earlier phase gates

- [ ] **C1 — Phase 3 mini SYNC+CINE**  
  Phase 3 `tasks.md` **5.2** (mini Play SYNC+CINE) remains **open** if unfinished — Phase 4 does not mark Phase 3 Done.

- [ ] **C2 — Phase 2 Chocomel weighted bar**  
  Phase 2 `tasks.md` **5.3** (Chocomel weighted idle↔walk Play acceptance) remains **open** if unfinished.

- [ ] **C3 — Phase 1 Chocomel hard-cut bar**  
  Phase 1 `tasks.md` **6.4** (Chocomel hard-cut Play acceptance) remains **open** if unfinished.

**Pass:** Phase 4 does not regress Phase 1–3 playback; earlier content gates not silently closed.

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
