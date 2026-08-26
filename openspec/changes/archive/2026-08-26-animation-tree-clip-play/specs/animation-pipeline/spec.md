## ADDED Requirements

### Requirement: Tree blend specification may use Clip Play as base
When a Clip Play override is set, the Tree blend specification SHALL use that one clip as the base (then Fire-slot exclusive sample while occupying, then Add2). The Pipeline SHALL still accept exactly one specification per evaluate. This slice SHALL NOT introduce a unified BlendSpec algebra.

#### Scenario: Active Tree with Clip Play still excludes Player two-slot
- **WHEN** AnimationTree is active with a Clip Play override
- **THEN** Pipeline evaluate uses the Tree blend specification only
- **AND** Player two-slot sampling does not write Local Pose in that evaluate
