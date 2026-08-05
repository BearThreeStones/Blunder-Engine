# Manual checklist — DogWalk animation Phase 6

Human-run validation for tasks **6.2** / **7.2** (Edit scrub for the three product modifiers + lean Play feel). Automated gates (`dogwalk_phase6_engineering_gates_test`, `dogwalk_phase6_lean_play_acceptance_test`, per-modifier unit tests, `animation_preview_controller_test`, etc.) do **not** substitute for this pass.

**Status:** Not run — checklist authored with concrete paths. Check boxes when a human completes each step.

---

## Prerequisites

| Item | Value |
|------|--------|
| OpenSpec change | `openspec/changes/dogwalk-animation-phase-6/` |
| Engine worktree | `E:\Dev\Blunder-Engine\.worktrees\dogwalk-animation-phase-6` |
| Configure (once) | `cmake --preset vs2026-debug` (from worktree root) |
| Build (Debug) | `cmake --build build/vs2026-debug --config Debug --target engine_editor engine_player dogwalk_phase6_engineering_gates_test dogwalk_phase6_lean_play_acceptance_test` |
| Binaries | `build\vs2026-debug\bin\Debug\engine_editor.exe`, `engine_player.exe` |
| Automated gates (6.1 / 6.2) | `build\vs2026-debug\engine\src\tests\Debug\dogwalk_phase6_engineering_gates_test.exe`, `dogwalk_phase6_lean_play_acceptance_test.exe` |
| Test Project | `E:\Blunder Projects\Test` (or agreed harness / test field) |
| Edit host policy | **Do not** set `BLUNDER_DOTNET_SCRIPTS=1` for Edit preview (Behaviour Tick must stay off) |
| Prior gates registry | `openspec/changes/dogwalk-animation-phase-6/PRIOR-GATES-TRACKING.md` |

**Run automated gates first (tasks 6.1 + 6.2 engineering / lean bars):**

```powershell
cd E:\Dev\Blunder-Engine\.worktrees\dogwalk-animation-phase-6
cmake --build build/vs2026-debug --config Debug --target dogwalk_phase6_engineering_gates_test dogwalk_phase6_lean_play_acceptance_test
.\build\vs2026-debug\engine\src\tests\Debug\dogwalk_phase6_engineering_gates_test.exe
.\build\vs2026-debug\engine\src\tests\Debug\dogwalk_phase6_lean_play_acceptance_test.exe
```

1. Open the Test Project in `engine_editor.exe` from this worktree build.
2. Load a scene with a character Object that has Skeleton + AnimationPlayer (+ active AnimationTree when required for modifier resample) and the three Phase 6 product modifiers authored via Inspector: **PaperMouth**, **SkeletonAttachModifier**, **LookAt**.
3. Confirm modifier chain order, enable flags, and key params (jaw bone / `openAmount`, attach child + bone, look-at bone + target) round-trip from scene save/reload.

---

## A. Edit Mode scrub — three product modifiers (no Behaviour Tick)

- [ ] **A1 — PaperMouth `openAmount`**  
  In Edit Mode, scrub `openAmount` from closed → open. Jaw pose changes perceptibly without Behaviour Tick.

- [ ] **A2 — SkeletonAttachModifier child follow**  
  In Edit Mode, scrub host bone animation (or attach bone selection) while a child Object is bound. Child Transform follows the attachment bone world pose without Behaviour Tick.

- [ ] **A3 — LookAt aim**  
  In Edit Mode, scrub look-at target (or target Object). Head / configured bone aims at the target; retargeting changes aim without Behaviour Tick.

- [ ] **A4 — Modifier chain order (optional)**  
  Reorder enable/disable modifiers in Inspector; Edit preview reflects chain order. Visual Tree canvas not required.

**Pass:** Edit scrub covers all three Phase 6 product modifiers without DotNetHost / Behaviour Tick.

---

## B. Lean Play bars (modifier-only feel)

Automated lean Play (`dogwalk_phase6_lean_play_acceptance_test`) covers mouth open, child follow, and aim change on Play tick + Edit scrub paths. **Human Play** still required for product feel (viewport, real content clips, Inspector-authored scenes).

- [ ] **B1 — Visible mouth open**  
  Under Play, `openAmount` drives perceptible jaw open/close (script or Inspector-driven scalar OK).

- [ ] **B2 — Child prop follow**  
  Under Play, attached child Object follows host bone through locomotion / base clip sampling.

- [ ] **B3 — Aim change**  
  Under Play, retargeting look-at target produces perceptible head/aim change.

**Pass:** Lean Play bars feel intentional; leash / full Pinda parity / Tree canvas not required.

---

## C. Regression / earlier phase gates

Phase 6 does **not** close Phase 1–5 content Done. See **`PRIOR-GATES-TRACKING.md`** for the authoritative open-gate table and regression policy.

- [ ] **C1 — Phase 1–3 Chocomel / SYNC+CINE**  
  Phase 1 `6.4` / `6.5` / `7.2`, Phase 2 `5.3` / `6.2`, Phase 3 `5.2` remain **open** if unfinished — Phase 6 does not mark them Done.

- [ ] **C2 — Phase 4 Chocomel-subset**  
  Phase 4 `tasks.md` **6.2** remains **open** if unfinished. Human steps: `dogwalk-animation-phase-4/manual-checklist.md` §B.

- [ ] **C3 — Phase 5 lean Play A/C/D**  
  Phase 5 `tasks.md` **8.2** §B remains **open** if unfinished. Human steps: `dogwalk-animation-phase-5/manual-checklist.md` §B.

**Pass:** Phase 6 modifier lean Play/Edit does not substitute for earlier content gates; prior manual checklists §C remain source of truth.

---

## Sign-off

| Role | Name | Date | Notes |
|------|------|------|-------|
| Engineer | | | Automated gates 6.1 + 6.2 green |
| Content / QA | | | A + B + C human boxes checked |

When all sections pass, a human may treat Phase 6 lean content bars as validated in product. This file does **not** auto-complete prior-phase `tasks.md` checkboxes.
