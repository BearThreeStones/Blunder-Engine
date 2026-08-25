## Purpose

Inspector Add… is the single authorship picker for attaching Camera, animation hosts, Behaviours, and SkeletonModifiers to the current selection without collapsing ClassDB and ECS into one Component type.

## ADDED Requirements

### Requirement: One Add… picker
The Inspector SHALL provide a single **Add…** picker for the current selection. First-slice items SHALL be Unique attachments (Camera, Skeleton, AnimationPlayer, AnimationTree), Behaviour types from the Behaviour type catalog, and SkeletonModifier types. Mesh SHALL remain Content Browser spawn and SHALL NOT appear in Add…. The picker SHALL be a grouped flat list (Unique attachments, then Behaviours, then Skeleton Modifiers) with no search. An empty Behaviour catalog SHALL still show the Behaviours group with the existing build-Scripts hint.

#### Scenario: Open Add… on one entity
- **WHEN** exactly one entity is selected and the author opens Add…
- **THEN** the picker lists Unique attachments, Behaviour types (or the build-Scripts hint), and SkeletonModifier types, grouped in that order

#### Scenario: Mesh is not in Add…
- **WHEN** the author opens Add…
- **THEN** Mesh is not listed as an attachment to add

### Requirement: Unique attachments disable when present
Camera, Skeleton, AnimationPlayer, and AnimationTree SHALL exist at most once on the selected Object or entity. When a Unique attachment is already present, its Add… row SHALL stay visible and disabled. Behaviours and SkeletonModifiers SHALL remain addable (multiple allowed).

#### Scenario: Camera already present
- **WHEN** the selected entity has a Camera Component and the author opens Add…
- **THEN** the Camera row is visible and cannot be chosen

#### Scenario: Second Behaviour still allowed
- **WHEN** the selected Object already has a Behaviour and the author opens Add…
- **THEN** Behaviour types remain enabled

### Requirement: Add… requires a single selection
First-slice Add… SHALL require exactly one selected entity. With no selection or with multiple selected entities, Add… SHALL be disabled or otherwise refuse to apply.

#### Scenario: Multi-select
- **WHEN** more than one entity is selected
- **THEN** Add… does not add attachments

### Requirement: Parallel Add buttons are not the product path
The Inspector SHALL NOT keep separate Add Camera, Add Behaviour, or Add Skeleton Modifier buttons as the product add path. Adding those types SHALL go through Add….

#### Scenario: No standalone Add Camera button
- **WHEN** one entity without a Camera is selected
- **THEN** the author adds Camera from Add… rather than a dedicated Add Camera button

### Requirement: Host cascade on Add…
Adding AnimationPlayer SHALL create a Skeleton on the same Object if missing. Adding AnimationTree SHALL create AnimationPlayer (and thus Skeleton) on the same Object if missing. Adding Skeleton SHALL NOT create AnimationPlayer or AnimationTree. A newly added AnimationTree SHALL be empty and inactive, with no AnimationTree Asset GUID required. Add… SHALL NOT fill the AnimationPlayer clip map. Add… SHALL NOT drive a Skeleton on a different Object.

#### Scenario: Add AnimationPlayer without Skeleton
- **WHEN** the author adds AnimationPlayer to an Object that has no Skeleton
- **THEN** that Object has both Skeleton and AnimationPlayer, and the clip map is empty

#### Scenario: Add Skeleton alone
- **WHEN** the author adds Skeleton to an Object that has no AnimationPlayer
- **THEN** the Object has a Skeleton and does not gain AnimationPlayer or AnimationTree

#### Scenario: Add AnimationTree
- **WHEN** the author adds AnimationTree to an Object with no animation hosts
- **THEN** the Object has Skeleton, AnimationPlayer, and an empty inactive AnimationTree with no Asset GUID

### Requirement: Object materialization
Adding Skeleton, AnimationPlayer, AnimationTree, a Behaviour, or a SkeletonModifier SHALL create a bound Object when the selected entity has none. Adding Camera SHALL NOT create an Object.

#### Scenario: Add Camera on mesh-only entity
- **WHEN** the selected entity has no bound Object and the author adds Camera
- **THEN** the entity has a Camera Component and still has no bound Object

#### Scenario: Add AnimationPlayer on mesh-only entity
- **WHEN** the selected entity has no bound Object and the author adds AnimationPlayer
- **THEN** the entity has a bound Object hosting Skeleton and AnimationPlayer

### Requirement: Skeleton hydration from skinned mesh
When Add… creates a Skeleton on the selected Object (directly or via host cascade), the engine SHALL fill rest/bind from that entity’s skinned mesh Intermediate glTF onto the same Object when the mesh is skinned. A static mesh or a failed glTF read SHALL still yield an empty Skeleton; the Add SHALL NOT fail. Add… SHALL NOT expand a glTF child hierarchy as a side effect of creating a Skeleton.

#### Scenario: Skinned mesh hydrates
- **WHEN** the selected entity references a skinned mesh and the author adds AnimationPlayer
- **THEN** the Object’s Skeleton has rest/bind bones from that mesh’s Intermediate glTF

#### Scenario: Static mesh empty Skeleton
- **WHEN** the selected entity references a non-skinned mesh and the author adds Skeleton
- **THEN** the Object has an empty Skeleton and the Add succeeds

#### Scenario: No child hierarchy expansion
- **WHEN** the author adds Skeleton to a spawned mesh entity
- **THEN** the scene does not gain extra glTF child nodes solely because Skeleton was added

### Requirement: Add clip is not an Add… item
The AnimationPlayer Inspector section SHALL provide **Add clip**, which appends one empty name→GUID row to that player’s clip map. Clips SHALL NOT appear in Add…. Content Browser drop onto a clip GUID is not required for this slice.

#### Scenario: Empty map after Add Player
- **WHEN** the author adds AnimationPlayer and then Add clip
- **THEN** the clip map has one empty name and GUID row editable in the Inspector

### Requirement: Remove attachment
Unique attachments SHALL have a section Remove. Behaviours, SkeletonModifiers, and clip rows SHALL keep per-row Remove. Removing AnimationTree or AnimationPlayer SHALL NOT remove Skeleton. Remove Skeleton SHALL be disabled while AnimationPlayer, AnimationTree, or any SkeletonModifier remains on that Object.

#### Scenario: Remove Player keeps Skeleton
- **WHEN** the Object has Skeleton and AnimationPlayer and the author Removes AnimationPlayer
- **THEN** Skeleton remains

#### Scenario: Remove Skeleton blocked
- **WHEN** the Object has AnimationPlayer and the author tries to Remove Skeleton
- **THEN** Skeleton is not removed
