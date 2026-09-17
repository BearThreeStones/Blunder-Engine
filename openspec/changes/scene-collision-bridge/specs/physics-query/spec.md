# Spec Delta

## Purpose

Play and the editor share one physics query API so C# and edit-mode rays can hit scene colliders by layer mask, including Areas when requested.

## ADDED Requirements

### Requirement: Shared query API for Play and Edit
The engine SHALL expose raycast and shapecast against the Physics World that belongs to the queried SceneInstance. Editor edit-mode queries SHALL use that same API on the Live document World. Play / `engine_player` queries SHALL use the Play Process World. A GPU viewport pick SHALL NOT substitute for this API.

#### Scenario: Edit ray uses the physics World
- **WHEN** the editor issues an edit-mode ray against the open scene
- **THEN** the hit is computed by the physics query API
- **AND** not by mesh GPU pick as the product path

#### Scenario: Play ray uses the Play World
- **WHEN** a Behaviour in `engine_player` raycasts
- **THEN** the query runs on the Play Process Physics World

### Requirement: Ray and primitive shapecast
The query API SHALL support raycast, box shapecast, sphere shapecast, and capsule shapecast. The query shape SHALL be a primitive (line, box, sphere, or capsule). The query shape SHALL NOT be a triangle mesh. A primitive query SHALL be allowed to hit Static triangle-mesh colliders.

#### Scenario: Sphere cast hits a static box
- **WHEN** a sphere shapecast sweeps into a Static box whose layer is in the mask
- **THEN** the query reports a hit

#### Scenario: Box shapecast is available
- **WHEN** a box shapecast is issued along a path that intersects a Static capsule in mask
- **THEN** the query reports a hit

#### Scenario: Query shape is not trimesh
- **WHEN** a caller requests a shapecast whose shape is a triangle mesh
- **THEN** the engine does not perform that shapecast as a supported query shape

### Requirement: Mask filter
A query SHALL take a 32-bit mask. A collider SHALL be a candidate only when `(collider.layer & query.mask) != 0`. Groups SHALL NOT be applied as this filter.

#### Scenario: Mask miss
- **WHEN** a collider’s layer bit is 4 and the query mask has only bit 0
- **THEN** the query does not hit that collider

### Requirement: Collide with Areas is opt-in
Queries SHALL default to not hitting Area colliders. When collide-with-areas is true, Area colliders that pass the mask SHALL be hittable.

#### Scenario: Default misses Area
- **WHEN** a ray is cast with collide-with-areas false through an Area box
- **THEN** that Area is not reported as a hit

#### Scenario: Opt-in hits Area
- **WHEN** a ray is cast with collide-with-areas true through an Area box whose layer is in the mask
- **THEN** the query reports that Area as a hit

### Requirement: Hit payload
A closest-hit result SHALL include point, normal, distance in metres, whether the collider is Area, and the hit entity’s groups. When the hit entity has a bound Object, the C# hit SHALL also expose that Object.

#### Scenario: Hit carries groups without Object
- **WHEN** a ray hits an entity that has group `TerrainSnow` and no bound Object
- **THEN** the hit groups include `TerrainSnow`
- **AND** the C# Object handle is absent
