## Context

Phase 2 locked unified two-slot blend + TimeScale ([ADR 0020](../../../docs/adr/0020-animation-player-two-slot-blend.md)). DogWalk still needs multi-Object starts and short cinematic handoffs. Grilling locked **DogWalk animation Phase 3** as **SYNC + CINE** only ([ADR 0023](../../../docs/adr/0023-animation-sync-group.md); CONTEXT Sync Group / SYNC / CINE / Phase 3). Stakeholders: runtime animation, input suppression hooks, C-ABI / Blunder.Api, Edit preview, Test Project content. Phase 2 Chocomel weighted Done may still be open; Phase 3 engine work may proceed in parallel.

## Goals / Non-Goals

**Goals:**

- **Sync Group** runtime: create / join / fire / release; Fire with per-member clip instructions; default hard cut
- **CINE** Enter + explicit End; in-CINE mark; optional gameplay-input suppression; C# owns pose/state
- Edit preview: Fire + Enter/End visibility without Behaviour / auto-TRS
- Engineering gate + mini multi-Object Play acceptance; parallel with Phase 2 content gate

**Non-Goals:**

- AnimationTree / BlendSpace / state machine / additive / procedural modifiers
- Cutscene Director / shared sample clock across members
- Serialized Sync Group member graphs as the required product path
- Cancelling Phase 2 Chocomel weighted Done criteria

## Decisions

1. **Phase 3 = SYNC + CINE** (not state graph / additive)  
   Early roadmap P3/P5 buckets reordered: SYNC/CINE ahead of AnimationTree.  
   *Alternatives:* state machine as Phase 3 (deferred); additive first (deferred).

2. **Sync Group is a runtime service (ADR 0023)**  
   Script-owned lifetime; not required scene-component member lists; no master/follower Player.  
   *Alternatives:* scene-baked groups; ad-hoc same-Tick Play; shared continuous clock.

3. **Fire = per-member instructions**  
   Heterogeneous clip names first-class; same-name Fire optional sugar.  
   *Alternatives:* identical name only; pre-bind clips on join only.

4. **Default Fire hard cut**  
   Aligns logical start moment; Crossfade not Sync Group default.  
   *Alternatives:* group-wide fade; per-member fade as default.

5. **CINE = segment contract, not director**  
   Thin Enter/End + in-CINE / input suppress; pose + gameplay state in C#.  
   *Alternatives:* naming-only; full cutscene timeline; engine auto TRS snap/restore.

6. **Explicit End is authoritative**  
   Scripts call End; member `finished` may assist.  
   *Alternatives:* lead-clip finished only; all-members finished; fixed duration.

7. **Edit preview = visible engine state (A)**  
   Fire + Enter/End + in-CINE marks + multi-Skeleton play; no Behaviour Tick; no auto TRS.  
   *Alternatives:* Play-only SYNC/CINE; Edit marker snap; Edit Behaviour subset.

8. **Done = engineering + mini Play bar; parallel Phase 2**  
   Simplified props OK; full shovel/snowman ending not required for Phase 3 Done.  
   *Alternatives:* engineering-only; require full DogWalk CINE sequence.

9. **Cross-Object Skeleton drive still out**  
   Sync Group coordinates multiple co-located Player↔Skeleton pairs.  
   *Alternatives:* remote Skeleton drive (rejected; stays post–Phase 3).

## Risks / Trade-offs

- [Edit “looks synced” vs Play handoff] → Mini Play bar remains mandatory Done; Edit does not fake TRS
- [Input suppression scope] → Define which actions/channels at apply-time; document defaults
- [Fire while member mid-Crossfade] → Hard-cut snap semantics; tests for interrupt
- [Phase 2 content still open] → Track gates separately; do not rewrite Phase 2 Done
- [Prop without Skeleton] → Phase 3 acceptance prefers skinned or TRS-animated props that use AnimationPlayer; pure static props out of sync gate

## Migration Plan

1. Land ADR 0023 + OpenSpec change; implement Sync Group + CINE hooks
2. C-ABI / Blunder.Api; Edit preview affordances
3. Automated tests (Fire alignment, hard cut, Enter/End, in-CINE)
4. Engineering gate scene; mini Play acceptance scene
5. Keep Phase 2 Chocomel weighted acceptance tracked separately
6. Rollback: remove Sync Group / CINE APIs; Phase 1/2 single-Player paths remain

## Open Questions

- Exact C-ABI symbol names / ABI version bump (apply-time)
- Whether Sync Group handles are opaque IDs vs named registry keys (apply-time)
- Which gameplay input channels in-CINE suppresses by default (apply-time; must match Input action model)
- Edit UI shape (toolbar vs Inspector) — Done does not require polished UI alone if API + harness prove preview
