## ADDED Requirements

### Requirement: Skinning consumes Pipeline Matrix Palette
CPU Fast Path skinning and GPU Final skinning SHALL consume the Animation Pipeline **Matrix Palette** (Global Pose × inverse bind per joint) produced at evaluate stage 6. The scene render bridge / draw path SHALL NOT be the authoritative author of that palette for animated Skeletons that have completed Pipeline evaluate.

#### Scenario: GPU path uses Pipeline palette
- **WHEN** a skinned mesh with cooked Final skin is drawn after Pipeline evaluate for its Skeleton
- **THEN** the uploaded bone palette matches the Pipeline Matrix Palette for that evaluate (within float tolerance), not a silently divergent draw-time rebuild from a pre-modifier Local Pose

#### Scenario: CPU Fast Path uses Pipeline palette
- **WHEN** Intermediate Fast Path CPU skinning runs after Pipeline evaluate
- **THEN** deformed positions are produced from the Pipeline Matrix Palette (or an equivalent consumer of that buffer)

### Requirement: Stale palette after Local-writing modifiers is forbidden
After SkeletonModifiers that write Local Pose, the Matrix Palette used for skinning SHALL be rebuilt from the post–stage-5 Global Pose before draw.

#### Scenario: LookAt then skinned draw
- **WHEN** LookAt writes a bone Local Pose and the skinned mesh is drawn in the same frame after evaluate
- **THEN** the skinned result reflects the LookAt-adjusted Global Pose (no one-frame stale pre-LookAt palette as the product path)
