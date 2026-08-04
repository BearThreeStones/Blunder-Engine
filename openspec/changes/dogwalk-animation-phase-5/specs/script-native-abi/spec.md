## ADDED Requirements

### Requirement: Blunder.Api Phase 5 animation surface
Blunder.Api SHALL expose managed façades for SkeletonModifier (as applicable), method-track subscription or event surface, BlendSpace2D parameters, and AnimationTree Asset reference consistent with the C-ABI. NativeAbi completeness for these entries SHALL be tested.

#### Scenario: Script sets 2D blend and receives method event
- **WHEN** a Play-mode Behaviour sets BlendSpace2D parameters and a method key is crossed on the dominant clock
- **THEN** the Behaviour can observe the method event and the Skeleton reflects the 2D-blended pose (with modifiers if any)
