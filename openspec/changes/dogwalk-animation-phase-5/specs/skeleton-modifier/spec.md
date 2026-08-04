## ADDED Requirements

### Requirement: SkeletonModifier ordered chain
The engine SHALL provide a **SkeletonModifier** ClassDB surface that runs **after** AnimationPlayer / AnimationTree sampling and **before** PoseApplied. Multiple modifiers on an Object SHALL form an ordered chain. Phase 5 Gate A SHALL include an extension point, a test double, and one minimal LookAt/aim-style sample modifier. SkeletonModifier SHALL NOT be equated with Add2 tree additive layers.

#### Scenario: Post-pose before PoseApplied
- **WHEN** an active AnimationTree (or Player) samples a pose and a SkeletonModifier is enabled
- **THEN** the modifier mutates the Skeleton after sample and PoseApplied observers see the post-modifier pose

#### Scenario: Ordered chain
- **WHEN** two SkeletonModifiers are registered in order A then B
- **THEN** A runs before B on the same sample frame
