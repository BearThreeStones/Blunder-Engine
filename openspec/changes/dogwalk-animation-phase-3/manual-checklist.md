# Manual checklist — DogWalk animation Phase 3

Human-run validation for task **6.2**. Automated gates do **not** substitute for this pass.

**Status:** Not run — checklist authored from grilling only. Fill build/scene paths when the apply milestone lands.

---

## Prerequisites

| Item | Value |
|------|--------|
| OpenSpec change | `openspec/changes/dogwalk-animation-phase-3/` |
| Engine build | TBD after apply (`engine_editor` / `engine_player`) |
| Test / harness scene | TBD — multi AnimationPlayer character + prop/partner |
| Edit host policy | **Do not** set `BLUNDER_DOTNET_SCRIPTS=1` for Edit preview (Behaviour Tick must stay off) |

1. Open the Test Project (or harness project) in the Phase 3 build.
2. Load the Phase 3 engineering / mini acceptance scene.
3. Confirm at least two Objects with co-located Skeleton + AnimationPlayer (character + prop/partner).

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
  If still open, remains tracked separately — Phase 3 mini acceptance does **not** mark Phase 2 Done.

**Pass:** Phase 3 does not regress Phase 1/2 playback; Phase 2 content gate not silently closed.
