# skeleton-hydration-on-instantiate Specification

## Purpose

Scene document instantiate fills empty Skeletons from the entity mesh after attach so reopen, Play, and Reload are not stuck in rest T-pose. Bones stay out of the scene file.

## Requirements

### Requirement: Instantiate hydrates empty Skeletons from the entity mesh
When a scene document is instantiated for editor Live, Player, Reload from disk, or a Scene Thumbnail still, the engine SHALL fill each empty Skeleton on an Object that already has AnimationPlayer or AnimationTree from that entity’s skinned mesh Intermediate glTF, after mesh attach. Mesh Asset References MAY be a GUID. Bones SHALL NOT be required in the scene file. A failed glTF read SHALL leave that Skeleton empty and SHALL NOT fail instantiate.

#### Scenario: Reopen a skinned Player or Tree scene
- **WHEN** the author opens a saved scene whose entity has a skinned mesh, `hasSkeleton`, and AnimationPlayer or AnimationTree
- **THEN** that Object’s Skeleton SHALL have named rest/bind bones from the mesh Intermediate glTF without Add… Skeleton in that session

#### Scenario: GUID mesh reference
- **WHEN** the entity mesh field is a Mesh Asset GUID that resolves to a skinned Intermediate glTF
- **AND** the scene is instantiated
- **THEN** that Object’s Skeleton SHALL have named bones from that glTF

#### Scenario: Failed mesh still instantiates
- **WHEN** the entity mesh cannot be resolved or has no skin
- **AND** the Object has AnimationPlayer or AnimationTree and an empty Skeleton
- **THEN** instantiate SHALL succeed
- **AND** that Skeleton SHALL remain empty

### Requirement: Skeleton-only and GEO children stay empty
Instantiate SHALL NOT fill an empty Skeleton on an Object that has neither AnimationPlayer nor AnimationTree. A child entity with `hasSkeleton` and no Player/Tree SHALL stay empty; skinning SHALL still use an ancestor Skeleton that has bones.

#### Scenario: Cube Skeleton without Player or Tree
- **WHEN** a scene entity has `hasSkeleton` and a mesh
- **AND** it has no AnimationPlayer and no AnimationTree
- **AND** the scene is instantiated
- **THEN** that Object’s Skeleton SHALL remain empty

#### Scenario: GEO child skins from parent
- **WHEN** a parent Object hydrates named bones
- **AND** a child has an empty Skeleton and no AnimationPlayer and no AnimationTree
- **THEN** the child’s Skeleton SHALL stay empty
- **AND** skinning for that child SHALL use the parent’s bones
