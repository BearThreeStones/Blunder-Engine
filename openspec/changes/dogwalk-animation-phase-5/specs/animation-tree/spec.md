## ADDED Requirements

### Requirement: Phase 5 hosts BlendSpace2D and Asset reference
AnimationTree SHALL host **BlendSpace2D** nodes and MAY reference an **AnimationTree Asset**. Sample order remains base (including BlendSpace2D states) then Add2, then **SkeletonModifier** chain, then PoseApplied.

#### Scenario: 2D state then modifiers
- **WHEN** the current state plays a BlendSpace2D and a SkeletonModifier is enabled
- **THEN** the tree produces the 2D-blended base (plus Add2 if any) and modifiers run before PoseApplied
