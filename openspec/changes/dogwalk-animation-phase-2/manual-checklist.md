# Manual checklist — DogWalk animation Phase 2

Human-run validation for task **6.2**. Automated gates (`dogwalk_phase2_blend_gate_test`, `animation_preview_controller_test`, etc.) do **not** substitute for this pass.

**Status:** Not run — checklist authored only. Check boxes when a human completes each step.

---

## Prerequisites

| Item | Value |
|------|--------|
| Engine worktree | `E:\Dev\Blunder-Engine\.worktrees\dogwalk-animation-phase-2` |
| Build (Debug) | `cmake --build build/vs2026-debug --config Debug --target engine_editor engine_player` |
| Test Project | `E:\Blunder Projects\Test` |
| Entry scene | `assets/Scenes/dogwalk_test_rig.scene.asset` |
| Behaviour | `Test.PlayerMove` on **TestRig** — `SetSlot(0, idle)`, `SetSlot(1, walk)`, `BlendWeight` from stick magnitude |
| Edit host policy | **Do not** set `BLUNDER_DOTNET_SCRIPTS=1` for Edit scrub (Behaviour Tick must stay off) |

1. Open the Test Project in `engine_editor.exe` from this worktree build.
2. Load `dogwalk_test_rig.scene.asset`.
3. Select the skinned **TestRig** entity (Object with co-located Skeleton + AnimationPlayer).

---

## A. Edit Mode scrub (no Behaviour Tick)

Goal: authorship viewport drives Phase 2 params via **Animation Preview Toolbar** without DotNetHost / Behaviour lifecycle.

The toolbar appears top-left in the viewport when the selection has an AnimationPlayer (`TS`, `BW`, `Fd`, `S0`, `S1`).

- [ ] **A1 — Toolbar visible**  
  With TestRig selected in **Edit Mode**, confirm the animation preview toolbar is shown (Play / Pause / Stop / Loop row + parameter row).

- [ ] **A2 — TimeScale (TS)**  
  Set **TS** to `0.5`, press **Play**, observe slower slot advance than at `1.0`. Set **TS** to `2.0`, confirm faster advance. Return to `1.0`.

- [ ] **A3 — Blend weight (BW)**  
  Ensure **S0** = `idle`, **S1** = `walk`. Scrub **BW** `0 → 0.5 → 1` while preview is playing (or after **Play** with Loop on). Skeleton pose should move continuously between idle and walk — not hard-cut-only.

- [ ] **A4 — Fade (Fd) + soft switch**  
  Set **BW** to `0`, **Fd** to `0.5`, **Play** `walk` (or toggle S1 and Play). Confirm a visible Crossfade ramp (weight moves toward walk over ~0.5 s). Set **Fd** to `0` and Play again — confirm immediate hard cut.

- [ ] **A5 — Slot assignment (S0 / S1)**  
  Edit **S0** / **S1** clip names to valid map entries; confirm preview resamples without crash. Restore `idle` / `walk`.

- [ ] **A6 — No Behaviour Tick**  
  While scrubbing in Edit Mode (steps A2–A5), confirm **no** Play session is active and `BLUNDER_DOTNET_SCRIPTS` is unset. Object position must **not** drift from WASD/gameplay (Behaviours are not ticking). Optional: debugger/log — no `Test.PlayerMove.Tick` activity during Edit preview.

**Pass:** TS, BW, Fd, and slots visibly affect Skeleton pose; no gameplay move or Behaviour Tick during Edit scrub.

---

## B. Play Mode — weighted idle↔walk + TimeScale + stepped facing

Goal: `Test.PlayerMove` drives two-slot blend under Play; position real-time; facing steps on dominant-slot clock.

1. Build `engine_player` from this worktree (editor Play toolbar spawns it beside the editor staging layout).
2. Enter **Play** from the editor toolbar (Player process, not in-editor DotNetHost).
3. Use move input (WASD / stick) on the test rig.

- [ ] **B1 — Weighted idle↔walk**  
  Hold still → mostly idle pose (`BlendWeight` ≈ 0). Move at partial stick → visible in-between blend. Full stick → mostly walk (`BlendWeight` ≈ 1). Deformation should follow the blend, not snap hard-cut only.

- [ ] **B2 — Real-time Position**  
  Character **translation** responds smoothly every frame while moving. Movement rate must **not** slow when animation TimeScale is changed (see B3) — gameplay Position is Tick real-time, independent of anim rate.

- [ ] **B3 — Perceptible TimeScale**  
  Stop Play. In Inspector, set `Test.PlayerMove.TimeScale` to `0.5`, Play again — walk/idle cycle clearly slower while move speed stays the same. Repeat with `2.0` — clearly faster. Restore `1.0`.

- [ ] **B4 — Stepped facing (dominant clock)**  
  While moving, visual facing updates in **discrete steps** aligned to animation timing (on-2s feel), not every render frame. Dominant slot: at high walk weight, step cadence should track walk playback; near idle weight, idle clock dominates. Gameplay yaw (stick) can lead visual yaw between steps.

- [ ] **B5 — Crossfade under script control (optional)**  
  If scene/script exposes `Play(walk, fade>0)`, confirm soft transition still respects two-slot model during Play.

**Pass:** Weighted blend, perceptible TimeScale, real-time move, stepped facing on dominant-slot `PlaybackPosition` via `PoseApplied`.

**Not in scope for this checklist:** Chocomel weighted acceptance (task **5.3** — blocked on import). Test-rig only here.

---

## C. Fast Path / Final skin — sane under blend

Goal: CPU Fast Path (Intermediate) and GPU Final (Cooked) both deform correctly at blended poses — no explode, T-pose, or wrong bind.

Reference: **Skinning path (Phase 1)** in `CONTEXT.md` — Fast Path = CPU from Intermediate; Final = GPU after Cook.

- [ ] **C1 — Fast Path (uncooked / CPU)**  
  With mesh Final missing or stale (fresh import or after invalidating Final — e.g. reimport clip or delete Cooked output if safe in a scratch project copy), open the scene in Edit or Play. At **BW** 0, 0.5, and 1, skinned mesh follows skeleton without exploded vertices, T-pose freeze, or gross bind-pose flash.

- [ ] **C2 — Final (Cooked / GPU)**  
  **Pull / Cook** the test-rig mesh (and dependent clips if required) so Final assets exist. Reload scene. Repeat blend sweep at **BW** 0, 0.5, 1 in Edit scrub and in Play. GPU path should match CPU qualitatively — no new artifacts introduced by Cook.

- [ ] **C3 — Blend extremes under motion**  
  In Play, sweep stick slowly `0 → 1 → 0` while moving. No pops that look like bind-pose or single-bone spikes; weights and slots stay coherent.

**Pass:** Skinned mesh remains plausible on Fast Path and Final across blend weights; no regression vs Phase 1 hard-cut-only deformation.

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
