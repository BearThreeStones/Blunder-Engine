## Context

Phases 1–6 locked AnimationPlayer, AnimationTree, SkeletonModifier, and skinning, but pose evaluation is split: Player/Tree write `Skeleton.pose_local`, modifiers mutate in place, Global is on-demand recursive, and `buildGpuBonePalette` runs in `scene_render_bridge` at draw time. [ADR 0032](../../../docs/adr/0032-animation-pipeline-evaluator.md) and CONTEXT define a first-class **Animation Pipeline**. Stakeholders: runtime animation, scene render bridge / skinning, Edit scrub, existing Phase 5–6 modifier tests.

## Goals / Non-Goals

**Goals:**

- Extract `AnimationPipeline::evaluate` (or equivalent) as the single pose math host for co-located animated Objects
- Explicit Global Pose + Matrix Palette buffers; PoseApplied after stage 6
- Player advance / active Tree sample / Edit scrub all call the same evaluate path
- Skinning consumes Pipeline palette
- Conservative stage 5 (always recompute Global after modifier chain)
- Tests proving single-clip, dual-slot, and LookAt → Global/palette consistency

**Non-Goals:**

- Unified BlendSpec type
- Lazy palette as default; world-batch Animation System
- PreGlobal/PostGlobal Modifier API split
- Replacing Player/Tree public Play/Travel APIs
- Closing DogWalk content Done gates
- New C-ABI bump solely for Pipeline (reuse existing surfaces)

## Decisions

1. **Pipeline co-located; Player schedules (ADR 0032)**  
   Evaluate runs on the Object’s animation path after clocks + blend specification are resolved.  
   *Alternatives:* Object-owned independent ticker; world Animation System batch.

2. **Local on Skeleton; Global + Palette as Pipeline buffers**  
   Keep `pose_local` as Local Pose; materialize Global matrices and skinning palette with dirty/invalidation.  
   *Alternatives:* Fully separate pose arenas; palette only in render bridge.

3. **Two near-term blend-specification shapes**  
   Player two-slot/Crossfade vs Tree base+Add2 feed one evaluate for stages 3–6; exactly one spec per evaluate.  
   *Alternatives:* Day-one unified BlendSpec; keep dual writers without a Pipeline host.

4. **Always stage 5 after Modifier chain**  
   Matches “A implement / B semantic”; skip optimization later via modifier intent metadata.  
   *Alternatives:* Intent flags in v1; Pre/Post hook split.

5. **Palette every evaluate; skinning reads it**  
   `buildGpuBonePalette` / CPU skinning take Pipeline output (or Skeleton accessor filled by Pipeline). Draw path must not silently rebuild as source of truth.  
   *Alternatives:* Leave authoritative build in render bridge; lazy-only.

6. **Global = Skeleton/model space; LookAt target = world**  
   Convert at LookAt apply entry using host Object Transform.  
   *Alternatives:* World Global Pose; dual model+world buffers.

7. **Migration via internal extract, not API break**  
   Refactor `sampleOntoSkeleton` / Tree sample / `applyModifiersThenNotifyPoseApplied` into evaluate stages; keep ClassDB Play/Tree drives stable.

## Risks / Trade-offs

- [Double palette work during transition] → Gate Done on render bridge consuming Pipeline buffer; remove duplicate authoritative build
- [Edit scrub / Tree path skips Pipeline] → Explicit tests that both call evaluate
- [LookAt space bug latent today] → World→model conversion + regression test with non-identity Object TRS
- [Always stage 5 cost] → Accept for v1; intent metadata follow-on
- [Buffer ownership on Skeleton vs Pipeline object] → Prefer Skeleton-held caches filled by Pipeline to minimize call-site churn; document accessor contract

## Migration Plan

1. Add Global/Palette cache + invalidate-on-Local-write on Skeleton (or Pipeline-owned buffers keyed by Skeleton)
2. Introduce evaluate: extract/blend (existing samplers) → modifiers → rebuild Global → build palette → PoseApplied
3. Route Player + Tree + Edit scrub through evaluate
4. Point CPU/GPU skinning at Pipeline palette
5. Fix LookAt world→model if needed
6. Tests; rollback = feature-flag or revert to direct sample + draw-time palette (pre-ADR path)

## Open Questions

- Exact type names (`AnimationPipeline` vs free functions) — lock at apply
- Whether palette stores glTF joint order vs bone-index order — follow existing `buildGpuBonePalette` / `MeshSkinData` contract
- Whether `getBoneGlobalPoseMatrix` becomes a thin read of the cache (recommended) or stays recursive until all callers migrate — recommend cache-backed read after evaluate
