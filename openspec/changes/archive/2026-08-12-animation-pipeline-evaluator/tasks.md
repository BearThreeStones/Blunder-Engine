## 1. Pose buffers and Pipeline host

- [x] 1.1 Add Global Pose cache + Matrix Palette buffer on Skeleton (or Pipeline-owned, Skeleton-keyed) with invalidate-on-Local-write
- [x] 1.2 Add `AnimationPipeline::evaluate` (or equivalent) hosting stages 1–6; wire existing clip sample / blend helpers for stages 1–2
- [x] 1.3 After Modifier chain, always rebuild Global (stage 5) then Matrix Palette (stage 6); fire PoseApplied only after stage 6
- [x] 1.4 Make `getBoneGlobalPoseMatrix` (or documented accessor) read the post-evaluate Global cache when valid

## 2. Player / Tree / Edit scrub routing

- [x] 2.1 Refactor AnimationPlayer `sampleOntoSkeleton` / advance path to build Player blend specification then call Pipeline evaluate
- [x] 2.2 Route active AnimationTree sample path through the same evaluate (Tree spec only; Player two-slot excluded)
- [x] 2.3 Confirm Edit Mode scrub paths call evaluate (no second ad-hoc sample→PoseApplied path)
- [x] 2.4 Preserve Sync Group Fire / OneShot behavior: still ends in Pipeline evaluate on the member Object

## 3. Skinning consumers

- [x] 3.1 Change GPU palette upload path to consume Pipeline Matrix Palette after evaluate
- [x] 3.2 Change CPU Fast Path skinning to consume the same Pipeline palette
- [x] 3.3 Remove or demote draw-time `buildGpuBonePalette` as authoritative for evaluated Skeletons (helper may remain for bind/rest-only thumbs if needed)

## 4. LookAt world target

- [x] 4.1 Convert LookAt product target from world space to Skeleton/model space at apply using host Object Transform
- [x] 4.2 Test: non-identity Object TRS + world target aims correctly; palette stays model-space (no Object translation baked in)

## 5. Tests and closeout

- [x] 5.1 Test: single-clip evaluate → Local / Global / palette consistency
- [x] 5.2 Test: dual-slot blend → stages 3–6 → PoseApplied ordering
- [x] 5.3 Test: LookAt Local write → stage 5/6 → skinned palette reflects post-LookAt Global
- [x] 5.4 Test: active Tree blocks Player two-slot Local writes for that evaluate
- [x] 5.5 Confirm CONTEXT Animation Pipeline terms + ADR 0032 match implementation names; note follow-ons (unified BlendSpec, modifier intent skip, lazy palette)

### Implementation names (5.5)

| Domain | Code |
|--------|------|
| Animation Pipeline finalize (stages 4–6) | `animationPipelineFinalize` / `animationPipelineRebuildOutputs` (`animation_pipeline.h`) |
| Global Pose + Matrix Palette buffers | `Skeleton::rebuildPoseBuffers`, `getBoneSkinMatrix`, `hasValidPoseBuffers` |
| LookAt host world | `SkeletonLookAtModifier::setHostWorldMatrix`, `Object::computeWorldMatrix` |

Follow-ons: unified BlendSpec; modifier intent skip stage 5; lazy palette default.

**Verify:** `animation_pipeline_test` OK (Debug).
