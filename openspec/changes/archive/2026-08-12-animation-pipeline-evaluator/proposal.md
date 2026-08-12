## Why

Clip sample, blend, SkeletonModifier, and skinning already exist, but evaluation is folded into AnimationPlayer / AnimationTree and the joint palette is rebuilt in the render bridge — so Global Pose / Matrix Palette / post-process recompute are not a named contract. Grilling locked a first-class **Animation Pipeline** ([ADR 0032](../../../docs/adr/0032-animation-pipeline-evaluator.md); [CONTEXT.md — Animation Pipeline](../../../CONTEXT.md)). We need the first reconstruction slice now so modifiers and skinning share one final pose boundary before further BlendSpec unification.

## What Changes

- Introduce a per-Object **Animation Pipeline** evaluator (`evaluate`) with stages: clip pose extract → pose blend → Global Pose → SkeletonModifier post-process → global recompute → Matrix Palette.
- **Local Pose** remains on **Skeleton**; **Global Pose** (Skeleton/model space) and **Matrix Palette** become explicit Pipeline buffers with invalidation after Local writes.
- **AnimationPlayer** remains the near-term **scheduler**; Player and AnimationTree produce **blend specifications** (separate near-term shapes; unified BlendSpec later). Active Tree excludes Player two-slot bone writes for that evaluate.
- **PoseApplied** fires only after stage 6 (palette) completes.
- After the Modifier chain, **always** recompute Global (conservative); semantic model allows later per-modifier skip.
- **Skinning** (CPU Fast Path / GPU Final) **consumes** the Pipeline Matrix Palette; draw bridge must not be the authoritative palette author.
- **LookAt** product target remains **world space**; convert to model space at modifier entry (may land in this change if not already correct).
- Engineering tests: single clip, dual-slot blend, LookAt-then-palette/Global consistency; Edit scrub and Tree-active paths must use the same Pipeline.

**Out of scope (this change):** unified BlendSpec algebra; lazy palette as the default path; world-batch Animation System host; PreGlobal/PostGlobal Modifier chain split; replacing AnimationPlayer / AnimationTree product APIs; changing DogWalk Phase 1–7 content Done gates; new C-ABI surface beyond what existing sample/modifier paths already need.

## Capabilities

### New Capabilities
- `animation-pipeline`: First-class evaluate contract, stage order, Local/Global/Matrix Palette products, Player scheduling, Player/Tree blend-specification inputs, PoseApplied-after-stage-6
- `animation-pipeline-skinning`: Skinning paths consume Pipeline Matrix Palette; render bridge is a consumer not the authoritative author

### Modified Capabilities
- `skeleton-modifier`: Runs as Pipeline stage 4; Local-writing modifiers invalidate Global; stage 5 runs before palette / PoseApplied (conservative always-recompute in this slice)

## Impact

- Engine: new Pipeline evaluate host; Skeleton Global/Palette buffers (or equivalent cache); AnimationPlayer / AnimationTree sample paths call evaluate; `scene_render_bridge` / `buildGpuBonePalette` consumers read Pipeline palette; LookAt world→model conversion if missing
- Tests: pipeline unit/integration coverage for blend + LookAt + palette consistency; regression that Tree-active and Edit scrub stay on Pipeline
- Docs: CONTEXT terms already updated; ADR 0032
- Non-impact: Play/C-ABI product Play surface shape; AnimationTree node set; Phase 6 modifier product types beyond timing/space fixes
- Follow-ons (not this change): unified BlendSpec; modifier intent metadata / skip stage 5; lazy palette for non-visible; world-batch scheduling
