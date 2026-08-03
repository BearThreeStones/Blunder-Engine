## Context

Phase 3 locked Sync Group + CINE ([ADR 0023](../../../docs/adr/0023-animation-sync-group.md)). DogWalk locomotion still needs BlendSpace1D, StateMachine/OneShot, and Add2 without full Godot AnimationTree parity. Grilling locked **DogWalk animation Phase 4** as lean **AnimationTree** ([ADR 0025](../../../docs/adr/0025-animation-tree-phase-4.md); CONTEXT AnimationTree / BlendSpace1D / StateMachine / OneShot / Add2 / Phase 4). Stakeholders: runtime animation, C-ABI / Blunder.Api, Edit preview, Test Project / Chocomel content. Phase 1–3 content Done gates may still be open; Phase 4 engine work may proceed in parallel.

## Goals / Non-Goals

**Goals:**

- AnimationTree ClassDB type, co-located with AnimationPlayer + Skeleton
- Exclusive Skeleton sampling when active; inactive falls back to Player two-slot path
- Lean nodes: StateMachine, BlendSpace1D, OneShot, Add2 (base then additive vs bind/rest)
- Narrow named script API; scene-embedded topology
- Sync Fire → OneShot when member tree is active
- PoseApplied + base dominant-clip step clock; Player TimeScale global on tree
- Edit scrub without Behaviour Tick
- Engineering gate + Chocomel-subset Play acceptance

**Non-Goals:**

- BlendSpace2D / Pinda-complete / nested Godot parity
- Procedural bone modifiers; method/audio tracks; Cubic/Bezier
- Visual AnimationTree editor or standalone Tree Asset as Done
- Godot parameter-path primary API; per-node TimeScale
- Cancelling Phase 1–3 content Done criteria
- Cross-Object Skeleton drive

## Decisions

1. **Phase 4 = AnimationTree lean graph (ADR 0025)**  
   BlendSpace1D + StateMachine/OneShot + Add2 — not player-hosted graph, not C#-only topology.  
   *Alternatives:* grow AnimationPlayer; script-only graph; additive-only milestone.

2. **Active tree exclusively samples Skeleton**  
   Player Play/two-slot do not write bones while active.  
   *Alternatives:* tree feeds two-slot; dual-write last-wins.

3. **Sync Fire → OneShot on active tree**  
   No-tree members keep hard-cut Play; do not deactivate tree by default.  
   *Alternatives:* Fire fails; Fire deactivates tree; Fire→travel only.

4. **Sample stack: base then Add2 (bind/rest)**  
   Base from StateMachine (BlendSpace1D or clip); OneShot inserts over base; Add2 additive on result.  
   *Alternatives:* free-form deep nesting; Add2 as lerp slot.

5. **Narrow named API; per-node BlendSpace scalars**  
   Travel/Start, SetBlendSpace by node logical name, RequestOneShot, Add2 setters.  
   *Alternatives:* Godot parameter paths; single anonymous global float.

6. **Scene-embedded topology**  
   No required visual editor / Tree Asset in Phase 4.  
   *Alternatives:* standalone AnimationTree Asset; runtime-only C# build.

7. **Step clock = base dominant clip**  
   PoseApplied retained; Add2 not the step source; TimeScale = Player global.  
   *Alternatives:* blended time clock; Tree-only TimeScale; drop PoseApplied under tree.

8. **Edit = scrub engine drives without Behaviour**  
   Aligns Phase 2/3 Edit philosophy; Stepped Play-validated.  
   *Alternatives:* Play-only tree; Edit runs Behaviour subset.

9. **Done = engineering + Chocomel-subset Play; parallel earlier gates**  
   Visible additive turn enough (exact Godot turn clip names not required).  
   *Alternatives:* engineering-only; full Pinda/Godot parity bar.

## Risks / Trade-offs

- [Fire OneShot vs authored SYNC states] → Document OneShot as default; travel sugar optional later; acceptance uses return-to-base
- [Scene-embedded graphs hard to reuse] → Accept for Phase 4; Tree Asset later if needed
- [Add2 reference pose mistakes] → Tests lock bind/rest deltas; document authored additive clips
- [Phase 1–3 content still open] → Track gates separately; do not rewrite earlier Done
- [Edit scrub UI polish] → Done does not require polished graph UI if API + harness prove scrub
- [Dominant neighbor ties in BlendSpace1D] → Define deterministic tie-break at apply-time; test it

## Migration Plan

1. Land ADR 0025 + OpenSpec change; implement AnimationTree sample stack
2. Wire exclusive active sampling; update Sync Fire→OneShot
3. C-ABI / Blunder.Api; scene serialization of embedded topology
4. Edit scrub affordances; automated tests
5. Engineering gate; Chocomel-subset Play acceptance
6. Keep Phase 1–3 content gates tracked separately
7. Rollback: remove/disable AnimationTree; Phase 1–3 Player / Sync paths remain

## Open Questions

- Exact C-ABI symbol names / ABI version bump (apply-time)
- Deterministic BlendSpace1D neighbor tie-break (apply-time)
- OneShot fade in/out durations defaults (apply-time; hard-cut insert OK for v1 if documented)
- How many concurrent Add2 layers in Phase 4 Done (suggest ≥1 required; N-bus optional)
- Edit UI shape (Inspector vs toolbar) — Done does not require polished UI alone if API + harness prove scrub
