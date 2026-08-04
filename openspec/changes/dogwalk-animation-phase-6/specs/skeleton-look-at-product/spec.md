## ADDED Requirements

### Requirement: Configurable LookAt product
The engine SHALL elevate LookAt from a Phase 5 sample to a configurable ClassDB SkeletonModifier product with at least configurable bone name and target, applied after sample and before PoseApplied. Phase 6 Done SHALL NOT require a generic C# SkeletonModifier subclass hot-bridge.

#### Scenario: Target change aims bone
- **WHEN** LookAt is enabled and its target changes
- **THEN** the configured bone orientation after the modifier differs and aims toward the target within the product's documented tolerance

#### Scenario: Edit scrub LookAt
- **WHEN** an author changes LookAt target or bone in Edit Mode without Behaviour Tick
- **THEN** preview sampling shows the updated aim result
