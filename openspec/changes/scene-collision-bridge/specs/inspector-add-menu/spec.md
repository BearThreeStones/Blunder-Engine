# Spec Delta

## MODIFIED Requirements

### Requirement: One Add… picker
The Inspector SHALL provide a single **Add…** picker for the current selection. Unique attachments SHALL be Camera, Light, Skeleton, AnimationPlayer, AnimationTree, Collider, and Character Controller. Behaviour types from the Behaviour type catalog and SkeletonModifier types remain listed. Mesh SHALL remain Content Browser spawn and SHALL NOT appear in Add…. The picker SHALL be a grouped flat list (Unique attachments, then Behaviours, then Skeleton Modifiers) with no search. An empty Behaviour catalog SHALL still show the Behaviours group with the existing build-Scripts hint.

#### Scenario: Open Add… on one entity
- **WHEN** exactly one entity is selected and the author opens Add…
- **THEN** the picker lists Unique attachments (including Light, Collider, and Character Controller), Behaviour types (or the build-Scripts hint), and SkeletonModifier types, grouped in that order

#### Scenario: Mesh is not in Add…
- **WHEN** the author opens Add…
- **THEN** Mesh is not listed as an attachment to add

### Requirement: Unique attachments disable when present
Camera, Light, Skeleton, AnimationPlayer, AnimationTree, Collider, and Character Controller SHALL exist at most once on the selected Object or entity. When a Unique attachment is already present, its Add… row SHALL stay visible and disabled. Behaviours and SkeletonModifiers SHALL remain addable (multiple allowed).

#### Scenario: Camera already present
- **WHEN** the selected entity has a Camera Component and the author opens Add…
- **THEN** the Camera row is visible and cannot be chosen

#### Scenario: Light already present
- **WHEN** the selected entity has a Light Component and the author opens Add…
- **THEN** the Light row is visible and cannot be chosen

#### Scenario: Collider already present
- **WHEN** the selected entity has a Collider Unique and the author opens Add…
- **THEN** the Collider row is visible and cannot be chosen

#### Scenario: Character Controller already present
- **WHEN** the selected entity has a Character Controller Unique and the author opens Add…
- **THEN** the Character Controller row is visible and cannot be chosen

#### Scenario: Second Behaviour still allowed
- **WHEN** the selected Object already has a Behaviour and the author opens Add…
- **THEN** Behaviour types remain enabled

### Requirement: Object materialization
Adding Skeleton, AnimationPlayer, AnimationTree, a Behaviour, or a SkeletonModifier SHALL create a bound Object when the selected entity has none. Adding Camera, Light, Collider, or Character Controller SHALL NOT create an Object.

#### Scenario: Add Camera on mesh-only entity
- **WHEN** the selected entity has no bound Object and the author adds Camera
- **THEN** the entity has a Camera Component and still has no bound Object

#### Scenario: Add Light on mesh-only entity
- **WHEN** the selected entity has no bound Object and the author adds Light
- **THEN** the entity has a Light Component and still has no bound Object

#### Scenario: Add Collider on mesh-only entity
- **WHEN** the selected entity has no bound Object and the author adds Collider
- **THEN** the entity has a Collider Unique and still has no bound Object

#### Scenario: Add Character Controller on mesh-only entity
- **WHEN** the selected entity has no bound Object and the author adds Character Controller
- **THEN** the entity has a Character Controller Unique and still has no bound Object

#### Scenario: Add AnimationPlayer on mesh-only entity
- **WHEN** the selected entity has no bound Object and the author adds AnimationPlayer
- **THEN** the entity has a bound Object hosting Skeleton and AnimationPlayer
