## Context

Phase 1 landed Skeleton + AnimationPlayer with hard-cut Play, PoseApplied, Constant/Linear sampling, and C# Stepped sync — but DogWalk locomotion needs continuous idle↔walk weighting and soft switches. Grilling locked **DogWalk animation Phase 2** as P2 only (no AnimationTree): unified two-slot blend + global TimeScale ([ADR 0020](../../../docs/adr/0020-animation-player-two-slot-blend.md)). Stakeholders: runtime animation, C-ABI / Blunder.Api, Edit preview UI, DogWalk/Test Project content. Phase 1 Chocomel hard-cut Done may still be open; Phase 2 engine work may proceed in parallel.

## Goals / Non-Goals

**Goals:**

- Unified **two-slot** AnimationPlayer: `SetSlot`, single `blendWeight ∈ [0,1]`, local TRS lerp / rotation slerp
- **Crossfade** as weight ramp on those slots; `Play(name, fade)` sugar; **hard cut** at `fade = 0`
- **Global TimeScale** scaling all slot advance
- Dominant-slot playback position for Animation step consumers; PoseApplied after combined sample
- Scene defaults for TimeScale / optional slots / initial weight; Edit scrub without Behaviour Tick
- C-ABI + Blunder.Api; engineering gate + Chocomel weighted Play Done

**Non-Goals:**

- AnimationTree / state machine / BlendSpace / N-way / additive layers / per-track TimeScale
- Method/audio tracks; procedural SkeletonModifier; SYNC/CINE; Cubic/Bezier
- In-editor Behaviour Tick; Edit Stepped-feel as Done gate
- Cancelling Phase 1 Chocomel hard-cut Done criteria

## Decisions

1. **Unified two slots (ADR 0020)**  
   One pipeline for continuous blend and Crossfade.  
   *Alternatives:* mutually exclusive modes; Crossfade as third sample path (rejected).

2. **Single blendWeight ∈ [0,1]**  
   0 = all slot0, 1 = all slot1.  
   *Alternatives:* dual independent weights (rejected Phase 2 complexity).

3. **Global TimeScale only**  
   One multiplier on the player.  
   *Alternatives:* per-track TimeScale (deferred).

4. **Local TRS blend**  
   Translation/scale lerp, rotation slerp.  
   *Alternatives:* additive/Add2 in Phase 2 (deferred).

5. **Dominant-slot step clock**  
   Frametime modulo uses higher-weight slot (or Crossfade target while fading).  
   *Alternatives:* blended time clock; explicit anchor clip API (deferred).

6. **Explicit slot APIs + Play sugar**  
   Primary: SetSlot / SetBlendWeight / TimeScale; Play(name, fade) convenience.  
   *Alternatives:* AnimationTree-style parameter paths (rejected).

7. **Scene defaults, not playback snapshots**  
   Persist TimeScale + optional default slots/weight with the name→GUID map.  
   *Alternatives:* runtime-only (hurts Edit); full playback snapshot (noise).

8. **Edit scrub without DotNetHost**  
   Toolbar/Inspector can drive TimeScale, fade, weights; Stepped still Play-only.  
   *Alternatives:* Play-only validation of blend (weaker authorship); full Edit=Play Stepped Done (rejected).

9. **Parallel with Phase 1 Chocomel hard-cut**  
   Engine Phase 2 may start before Phase 1 Chocomel hard-cut closes; both Done bars remain.  
   *Alternatives:* strict Phase 1-first (slower); subsume Phase 1 Done into Phase 2 (rewrites Phase 1).

## Risks / Trade-offs

- [Dominant-slot flips at weight 0.5] → Hysteresis optional later; document threshold; test Crossfade mid-ramp
- [PoseApplied reentrancy with Play fade] → Same Phase 1 rule: no re-entrant Play that re-samples mid-callback (or queue)
- [Phase 1 specs not archived] → Phase 2 deltas reference Phase 1 capability names; archive Phase 1 before or with Phase 2 carefully
- [Edit defaults vs Play script override] → Scene defaults are authorship start; Play scripts own runtime weights every Tick
- [Chocomel import cost shared] → Parallel tracks; one import serves hard-cut then weighted acceptance

## Migration Plan

1. Land ADR 0020 + OpenSpec change; implement two-slot sample/combine + TimeScale on AnimationPlayer
2. Extend C-ABI / Blunder.Api; bump NativeAbi completeness
3. Scene serialize defaults; Edit scrub controls
4. Automated tests (blend, Crossfade, TimeScale, dominant clock); test-rig engineering gate
5. Content: weighted idle↔walk + TimeScale on Chocomel/subset; keep Phase 1 hard-cut gate tracked separately
6. Rollback: revert change; Phase 1 hard-cut Play remains valid (`fade = 0` / single-slot equivalent)

## Open Questions

- Exact C-ABI symbol names / ABI version bump (apply-time)
- Dominant-slot tie-break when weights equal (pick slot0 vs last target — apply-time default)
- Whether Edit toolbar ships in the same apply milestone as runtime (Done does not require Edit UI completeness alone)
