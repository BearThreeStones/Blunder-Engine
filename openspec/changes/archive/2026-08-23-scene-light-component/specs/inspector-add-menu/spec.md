## MODIFIED Requirements

### Requirement: One Add… picker
The Inspector SHALL provide a single **Add…** picker for the current selection. Unique attachments SHALL be Camera, Light, Skeleton, AnimationPlayer, and AnimationTree. Behaviour types from the Behaviour type catalog and SkeletonModifier types remain listed. Mesh SHALL remain Content Browser spawn and SHALL NOT appear in Add…. The picker SHALL be a grouped flat list (Unique attachments, then Behaviours, then Skeleton Modifiers) with no search. An empty Behaviour catalog SHALL still show the Behaviours group with the existing build-Scripts hint.

#### Scenario: Open Add… on one entity
- **WHEN** exactly one entity is selected and the author opens Add…
- **THEN** the picker lists Unique attachments (including Light), Behaviour types (or the build-Scripts hint), and SkeletonModifier types, grouped in that order

#### Scenario: Mesh is not in Add…
- **WHEN** the author opens Add…
- **THEN** Mesh is not listed as an attachment to add

### Requirement: Unique attachments disable when present
Camera, Light, Skeleton, AnimationPlayer, and AnimationTree SHALL exist at most once on the selected Object or entity. When a Unique attachment is already present, its Add… row SHALL stay visible and disabled. Behaviours and SkeletonModifiers SHALL remain addable (multiple allowed).

#### Scenario: Camera already present
- **WHEN** the selected entity has a Camera Component and the author opens Add…
- **THEN** the Camera row is visible and cannot be chosen

#### Scenario: Light already present
- **WHEN** the selected entity has a Light Component and the author opens Add…
- **THEN** the Light row is visible and cannot be chosen

#### Scenario: Second Behaviour still allowed
- **WHEN** the selected Object already has a Behaviour and the author opens Add…
- **THEN** Behaviour types remain enabled

### Requirement: Object materialization
Adding Skeleton, AnimationPlayer, AnimationTree, a Behaviour, or a SkeletonModifier SHALL create a bound Object when the selected entity has none. Adding Camera or Light SHALL NOT create an Object.

#### Scenario: Add Camera on mesh-only entity
- **WHEN** the selected entity has no bound Object and the author adds Camera
- **THEN** the entity has a Camera Component and still has no bound Object

#### Scenario: Add Light on mesh-only entity
- **WHEN** the selected entity has no bound Object and the author adds Light
- **THEN** the entity has a Light Component and still has no bound Object

#### Scenario: Add AnimationPlayer on mesh-only entity
- **WHEN** the selected entity has no bound Object and the author adds AnimationPlayer
- **THEN** the entity has a bound Object hosting Skeleton and AnimationPlayer

## ADDED Requirements

### Requirement: Add… Light defaults to Directional
Adding Light from Add… SHALL create a Light Component whose type is Directional Light. Point, Spot, and Area SHALL NOT appear as separate Unique Add… rows. Changing type in the Inspector SHALL keep that same Unique Light Component.

#### Scenario: Add Light is Directional
- **WHEN** the author adds Light to an entity that has none
- **THEN** that entity has a Light Component of type Directional Light
