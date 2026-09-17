# Spec Delta

## Purpose

Authors attach one Collider Unique per entity so static world meshes and primitive volumes participate in the scene Physics World in SI metres.

## ADDED Requirements

### Requirement: Collider Unique on scene entities
The engine SHALL support a native Collider Unique on a scene entity (at most one per entity). Pose SHALL follow that entity’s TRS. Shape SHALL be a field on the Unique: Box, Sphere, Capsule, or Triangle Mesh. Body kind SHALL be Static, Kinematic, or Area. The Unique SHALL NOT be a C# Behaviour. Adding Collider SHALL NOT create a bound Object.

#### Scenario: Round-trip Collider Unique
- **WHEN** an entity has a Collider Unique with shape, body kind, layer, mask, and metre sizes
- **THEN** scene serialize and load preserve those fields

#### Scenario: Absent collider key
- **WHEN** an entity JSON object has no collider key
- **THEN** the loaded entity has no Collider Unique

### Requirement: Primitive shapes in metres
Box SHALL use half-extents in metres. Sphere SHALL use radius in metres. Capsule SHALL use radius and full height in metres (height includes hemispheres) with the capsule axis along entity local +Z. Sizes SHALL convert to Physics fixed scalars at identity scale (not centimetres).

#### Scenario: Identity scale box matches mesh metres
- **WHEN** a Static Box Collider has half-extents (1, 1, 1) on an identity TRS entity
- **THEN** the kernel box is one metre half-extents on each axis
- **AND** the shape is not scaled by 100

### Requirement: Triangle Mesh is Static only
Triangle Mesh SHALL attach only when body kind is Static. When shape is Triangle Mesh and body kind is Kinematic or Area, the engine SHALL attach no kernel collider for that Unique (skip, no auto-box).

#### Scenario: Static trimesh attaches
- **WHEN** a Static Triangle Mesh Collider Unique has a non-empty triangle mesh
- **THEN** that entity participates in collision as a static triangle mesh

#### Scenario: Moving trimesh is skipped
- **WHEN** a Collider Unique has shape Triangle Mesh and body kind Kinematic
- **THEN** that Unique contributes no kernel collider

### Requirement: Area is query-only
Area body kind SHALL NOT generate solid contacts that block Character Controller slides or Dynamic resting. Area colliders SHALL remain queryable when a query opts into collide-with-areas.

#### Scenario: Area does not block a slide
- **WHEN** a Character Controller capsule sweeps into an Area box
- **THEN** the slide is not stopped by that Area
- **AND** a ray with collide-with-areas true can still hit that Area

### Requirement: Collision layer and mask
Each Collider Unique SHALL store a 32-bit collision layer (what it is) and a 32-bit collision mask (what it sees). Default layer SHALL have bit 0 set. Default mask SHALL be all bits set. Inspector SHALL expose layers as bits 1 through 32.

#### Scenario: Default layer is bit 0
- **WHEN** the author adds a Collider Unique without editing layers
- **THEN** its layer has bit 0 set
- **AND** its mask includes all 32 bits

### Requirement: Add… Collider defaults
Adding Collider from Add… SHALL create a Static Box Collider Unique. Box, Sphere, Capsule, and Triangle Mesh SHALL NOT appear as four separate Unique Add… rows. Changing shape or body kind in the Inspector SHALL keep that same Unique.

#### Scenario: Add Collider is Static Box
- **WHEN** the author adds Collider to an entity that has none
- **THEN** that entity has a Collider Unique of shape Box and body kind Static

### Requirement: Product fixtures land in DogWalk
Authored collision fixtures for Human acceptance SHALL live in the DogWalk Project. The Test Project and Sponza SHALL NOT be this change's main scene. Engine unit tests MAY use synthetic scenes in the engine repository.

#### Scenario: DogWalk is the human scene
- **WHEN** the human walks the User stories
- **THEN** the open Project is DogWalk
- **AND** Test and Sponza are not required as that main scene
