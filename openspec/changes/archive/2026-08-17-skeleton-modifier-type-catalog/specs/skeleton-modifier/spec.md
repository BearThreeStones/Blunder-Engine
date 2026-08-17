## ADDED Requirements

### Requirement: Construct and load go through the type catalog
Constructing a SkeletonModifier from a type name (Inspector Add…, scene instantiate, and restore) SHALL use the SkeletonModifier type catalog. A type name that is not registered SHALL become a Missing SkeletonModifier that preserves the authored name. The engine MUST NOT replace an unknown type with a bare base SkeletonModifier instance.

#### Scenario: Unknown type is not coerced to base
- **WHEN** scene instantiate applies a skeletonModifiers entry whose type is not in the catalog
- **THEN** the chain slot is Missing and still reports the authored type name rather than `"SkeletonModifier"`
