# Manual checklist — DogWalk animation Phase 5

Human-run validation for tasks **8.2** (Edit A/C/D scrub + lean Play bars). Automated gates (`dogwalk_phase5_engineering_gates_test`, `dogwalk_phase5_lean_play_acceptance_test`, Gate A/C/D unit tests, `animation_preview_controller_test`, etc.) do **not** substitute for this pass.

**Status:** Not run — checklist authored with concrete paths. Check boxes when a human completes each step.

---

## Prerequisites

| Item | Value |
|------|--------|
| OpenSpec change | `openspec/changes/dogwalk-animation-phase-5/` |
| Engine worktree | `E:\Dev\Blunder-Engine\.worktrees\dogwalk-animation-phase-5` |
| Configure (once) | `cmake --preset vs2026-debug` (from worktree root) |
| Build (Debug) | `cmake --build build/vs2026-debug --config Debug --target engine_editor engine_player dogwalk_phase5_engineering_gates_test dogwalk_phase5_lean_play_acceptance_test` |
| Binaries | `build\vs2026-debug\bin\Debug\engine_editor.exe`, `engine_player.exe` |
| Automated gates | `build\vs2026-debug\engine\src\tests\Debug\dogwalk_phase5_engineering_gates_test.exe`, `dogwalk_phase5_lean_play_acceptance_test.exe` |
| Test Project | `E:\Blunder Projects\Test` (or agreed harness / test field) |
| Edit host policy | **Do not** set `BLUNDER_DOTNET_SCRIPTS=1` for Edit preview (Behaviour Tick must stay off) |

**Run automated gates first:**

```powershell
cd E:\Dev\Blunder-Engine\.worktrees\dogwalk-animation-phase-5
cmake --build build/vs2026-debug --config Debug --target dogwalk_phase5_engineering_gates_test dogwalk_phase5_lean_play_acceptance_test
.\build\vs2026-debug\engine\src\tests\Debug\dogwalk_phase5_engineering_gates_test.exe
.\build\vs2026-debug\engine\src\tests\Debug\dogwalk_phase5_lean_play_acceptance_test.exe
```

---

## A. Edit Mode scrub (no Behaviour Tick)

- [ ] **A1 — SkeletonModifier**  
  Enable/order a LookAt (or test) modifier; Edit preview shows post-pose change without Behaviour Tick.

- [ ] **A2 — BlendSpace2D**  
  Scrub named `(x,y)` on a BlendSpace2D node; perceptible blend change without Behaviour Tick.

- [ ] **A3 — Tree Asset + overrides**  
  Bind AnimationTree Asset GUID, apply topology, scrub allowlisted overrides (scalars / 2D params / add2Weight / currentState / active). Preview updates without Behaviour Tick.

- [ ] **A4 — Method markers (optional)**  
  Method keys may appear as Edit markers/logs; real Behaviour handling is Play-validated (B1).

**Pass:** Edit scrub covers A/C/D engine state without DotNetHost / Behaviour Tick; visual canvas not required.

---

## B. Lean Play bars (per gate)

Automated lean Play (`dogwalk_phase5_lean_play_acceptance_test`) covers LookAt+method, BlendSpace2D corners, and Asset+override. **Human Play** still required for product feel (Behaviours, viewport, content clips).

- [ ] **B1 — Lean Play A**  
  Visible post-pose modifier effect + observable method dispatch (Message / Behaviour) under Play.

- [ ] **B2 — Lean Play C**  
  Perceptible BlendSpace2D blend (Pinda subset or test field OK — full Pinda not required).

- [ ] **B3 — Lean Play D**  
  Asset reference + Inspector edit + instance override without needing Behaviour Tick for the override itself.

**Pass:** All three lean Play bars feel intentional; full leash/paper-mouth / canvas parity not required.

---

## C. Regression / earlier phase gates

- [ ] **C1 — Phase 4 Chocomel-subset**  
  Phase 4 `tasks.md` **6.2** remains **open** if unfinished — Phase 5 does not mark Phase 4 Done.

- [ ] **C2 — Phase 3 mini SYNC+CINE**  
  Phase 3 content gate remains tracked separately if open.

- [ ] **C3 — Phase 2 weighted / Phase 1 hard-cut**  
  Earlier Chocomel Done bars remain in force; not silently replaced by Phase 5.

**Pass:** Phase 5 does not regress Phase 1–4 playback; earlier content gates not silently closed.

---

## Sign-off

| Role | Name | Date | Notes |
|------|------|------|-------|
| Engineer | | | Automated gates green |
| Content / QA | | | A + B + C human boxes checked |
