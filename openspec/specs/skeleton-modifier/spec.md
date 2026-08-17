# skeleton-modifier Specification

## Purpose
SkeletonModifier as Animation Pipeline stage 4, with Global recompute before palette/PoseApplied, and LookAt world-space product targets.
## Requirements
### Requirement: SkeletonModifier is Pipeline stage 4
SkeletonModifiers SHALL run as Animation Pipeline **stage 4** — after Local Pose extract/blend from the active blend specification and before Global recompute (stage 5), Matrix Palette (stage 6), and PoseApplied. The ordered chain semantics from Phase 5 remain: enabled modifiers run in registration order.

#### Scenario: Chain between blend and PoseApplied
- **WHEN** Player or Tree blend produces a Local Pose and two enabled modifiers A then B are on the Object
- **THEN** A runs then B on that Local Pose before stage 5/6 and before PoseApplied

### Requirement: Local writes require Global recompute before consumers
A SkeletonModifier that writes Skeleton Local Pose SHALL invalidate Global Pose. The Pipeline SHALL recompute Global Pose (stage 5) before Matrix Palette and before PoseApplied. This slice SHALL always run stage 5 after the Modifier chain completes.

#### Scenario: LookAt invalidates then rebuilds Global
- **WHEN** LookAt reads Global Pose and writes Local Pose on the aim bone
- **THEN** subsequent Global Pose and Matrix Palette used by consumers match the post-LookAt Local Pose

### Requirement: LookAt target world space at product surface
Configurable LookAt SHALL treat the authored/script **target** as a **world-space** point. At apply time the engine SHALL convert the target into Skeleton/model space using the host Object Transform before aiming. Pipeline Global Pose SHALL remain model space.

#### Scenario: Non-identity Object transform aim
- **WHEN** the host Object has non-identity world TRS and LookAt target is set to a world-space point
- **THEN** the aimed bone orients toward that world point (within tolerance) after evaluate, without baking Object TRS into Matrix Palette

### Requirement: Construct and load go through the type catalog
Constructing a SkeletonModifier from a type name (Inspector Add…, scene instantiate, and restore) SHALL use the SkeletonModifier type catalog. A type name that is not registered SHALL become a Missing SkeletonModifier that preserves the authored name. The engine MUST NOT replace an unknown type with a bare base SkeletonModifier instance.

#### Scenario: Unknown type is not coerced to base
- **WHEN** scene instantiate applies a skeletonModifiers entry whose type is not in the catalog
- **THEN** the chain slot is Missing and still reports the authored type name rather than `"SkeletonModifier"`

