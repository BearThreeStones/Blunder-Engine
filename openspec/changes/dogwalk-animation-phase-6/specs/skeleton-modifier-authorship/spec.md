## ADDED Requirements

### Requirement: Scene serialization of product modifiers
The engine SHALL persist Phase 6 product SkeletonModifier types (PaperMouth, SkeletonAttachModifier, LookAt) with their order and product parameters on the Object / scene document so reload restores the chain without Behaviour assembly.

#### Scenario: Round-trip modifiers
- **WHEN** a scene with the three product modifiers configured is saved and reloaded
- **THEN** modifier types, order, enabled flags, and key parameters match the saved configuration

### Requirement: Inspector authorship
Authors SHALL be able to add, remove, reorder, enable/disable, and edit key parameters of Phase 6 product modifiers in the Inspector without a visual AnimationTree canvas and without requiring DotNetHost.

#### Scenario: Inspector edits PaperMouth
- **WHEN** an author sets PaperMouth `openAmount` and jaw bone in the Inspector
- **THEN** Edit preview reflects the change after sample
