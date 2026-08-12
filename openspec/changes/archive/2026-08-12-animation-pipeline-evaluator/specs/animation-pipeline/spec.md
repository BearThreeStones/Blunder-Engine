## ADDED Requirements

### Requirement: Animation Pipeline evaluate contract
The engine SHALL provide a per-animated-Object **Animation Pipeline** evaluation path that turns clip clocks plus a blend specification into a **Local Pose**, then **Global Pose** and **Matrix Palette**. The Pipeline SHALL execute stages in order: (1) clip pose extract, (2) pose blend when multiple inputs apply, (3) Global Pose from Local Pose and Skeleton hierarchy, (4) SkeletonModifier chain, (5) Global Pose recompute, (6) Matrix Palette generation. AnimationPlayer and AnimationTree SHALL supply clocks and blend specification; they SHALL NOT be treated as the Pipeline itself.

#### Scenario: Single-clip evaluate produces Local Global and palette
- **WHEN** a co-located AnimationPlayer plays one mapped clip and the Pipeline evaluates
- **THEN** the Skeleton Local Pose matches the sampled clip, Global Pose is available in Skeleton/model space, and a Matrix Palette exists for skinning consumers

#### Scenario: Dual-slot blend then final buffers
- **WHEN** both Player slots have clips and blend weight is between 0 and 1
- **THEN** Local Pose is the local TRS blend of the two extracts and stages 3–6 run on that result before PoseApplied

### Requirement: Player schedules Pipeline
The co-located AnimationPlayer advance path and the active AnimationTree sample path SHALL invoke Pipeline evaluate after resolving clocks and the active blend specification. Edit Mode scrub that updates skeleton pose SHALL use the same evaluate path. When an AnimationTree is active and blocking Player sampling, exactly the Tree blend specification SHALL feed evaluate for that Object.

#### Scenario: Active Tree excludes Player two-slot writes
- **WHEN** AnimationTree is active and blocking Player sampling
- **THEN** Pipeline evaluate uses the Tree blend specification only and Player two-slot sampling does not write Local Pose in that evaluate

#### Scenario: Edit scrub uses Pipeline
- **WHEN** Edit Mode scrubs AnimationPlayer or AnimationTree drives without Behaviour Tick
- **THEN** Skeleton Local/Global/Matrix Palette are updated through Pipeline evaluate (not a separate ad-hoc sample path)

### Requirement: Global Pose model space and invalidation
**Global Pose** SHALL be the per-bone pose in Skeleton/model space (hierarchy over Local Pose only; Object world TRS MUST NOT be baked into Global Pose or Matrix Palette). After Local Pose changes from blend or from SkeletonModifiers that write Local Pose, Global Pose SHALL be treated invalid until stage 5 recomputes it. This slice SHALL recompute Global Pose after every Modifier chain completion (conservative).

#### Scenario: Object translation does not enter palette
- **WHEN** the host Object world translation changes but Local Pose is unchanged and Pipeline has evaluated
- **THEN** Matrix Palette joint matrices remain free of that Object translation (model-space Global × inverse bind)

#### Scenario: Post-modifier Global is final before palette
- **WHEN** a Local-writing SkeletonModifier runs during stage 4
- **THEN** stage 5 recomputes Global Pose before stage 6 builds Matrix Palette

### Requirement: PoseApplied after Matrix Palette
PoseApplied listeners SHALL be notified only after Pipeline evaluate has completed through Matrix Palette generation (stage 6) for that Object.

#### Scenario: PoseApplied sees post-modifier final pose
- **WHEN** LookAt (or equivalent Local-writing modifier) is enabled and evaluate completes
- **THEN** PoseApplied observers observe Local/Global consistent with the post-modifier pose and a Matrix Palette built from that Global Pose

### Requirement: Near-term dual blend specifications
The Pipeline SHALL accept a Player blend specification (two-slot / Crossfade local TRS blend) or a Tree blend specification (AnimationTree base then Add2, etc.) in this slice. A unified BlendSpec algebra is NOT required for Done. Exactly one specification SHALL feed each evaluate.

#### Scenario: One spec per evaluate
- **WHEN** Pipeline evaluate runs for an Object
- **THEN** either the Player or the Tree blend specification is applied, never both writing Local Pose in the same evaluate
