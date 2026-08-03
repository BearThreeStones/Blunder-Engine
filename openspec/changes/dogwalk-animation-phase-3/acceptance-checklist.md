# Acceptance checklist — DogWalk animation Phase 3

Human-oriented Done bar for grilled Phase 3 (SYNC + CINE). Complements `tasks.md` and `manual-checklist.md`.  
**Status:** Draft from grilling — not executed. Check boxes when a human completes each item after implement.

## Product Done (must all pass)

- [ ] **P1 — Sync Group Fire**  
  Two or more AnimationPlayers start (or seek) at the same logical moment from one Fire with **different** clip logical names.

- [ ] **P2 — Hard cut default**  
  Fire snaps members without Sync-Group-driven Crossfade.

- [ ] **P3 — CINE Enter / End**  
  Enter sets in-CINE; explicit End clears it; clip `finished` alone does not End.

- [ ] **P4 — Input / control**  
  Optional gameplay-input suppression while in-CINE; End restores; pose/state handoff owned by C# Behaviours on Play (CONTEXT **CINE**; `cine_segment_service.h`).

- [ ] **P5 — Edit preview**  
  Edit can Fire + Enter/End; in-CINE visible; multi-Skeleton plays; **no** Behaviour Tick; **no** auto Object TRS snap.

- [ ] **P6 — Mini Play acceptance**  
  Character + prop/partner sync start + CINE handoff returning control (simplified props OK).

- [ ] **P7 — Parallel tracking**  
  Phase 2 Chocomel weighted acceptance still tracked if open (not dropped by Phase 3).

## Explicitly not required for Phase 3 Done

- Full AnimationTree / BlendSpace / state machine
- Additive layers / procedural bone modifiers
- Full Cutscene Director / timeline
- Full shovel–snowman ending (or other full DogWalk CINE) as the only bar
- Shared continuous playback clock across Sync Group members
- Cross-Object Skeleton drive

## References

- CONTEXT: Sync Group, SYNC, CINE, DogWalk animation Phase 3
- [ADR 0023](../../../docs/adr/0023-animation-sync-group.md)
- OpenSpec change: `openspec/changes/dogwalk-animation-phase-3/`
