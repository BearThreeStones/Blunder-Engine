# scene-edit-commands Specification

## Purpose
MVP Editor Commands for scene editing: transform commits, spawn, soft-delete with stable EntityId, and save-time tombstone filtering.
## Requirements
### Requirement: Transform commit is one Editor Command
Confirming a gizmo drag, confirming a Translate Modal session, or committing an Inspector TRS field SHALL push a single transform Editor Command that can restore the pre-commit local TRS on undo and the post-commit TRS on redo. Intermediate pointer moves or scrub values during the interaction SHALL NOT each push a Command.

#### Scenario: Modal confirm then undo
- **WHEN** the user confirms a Translate Modal that moved an entity and then undoes
- **THEN** the entity’s local transform matches the pre-modal start pose

#### Scenario: Modal cancel pushes nothing
- **WHEN** the user cancels a Translate Modal
- **THEN** Document History does not gain a new transform Command for that session

### Requirement: Spawn is an undoable Editor Command
Successfully spawning an entity (e.g. mesh spawn into the active scene) SHALL push an Editor Command. Undo SHALL soft-delete or otherwise reverse the spawn so the entity is not part of the editable document; redo SHALL restore it with the same EntityId when using soft-delete semantics.

#### Scenario: Undo spawn
- **WHEN** the user spawns a mesh entity and then undoes
- **THEN** the spawned entity is not visible in the editable hierarchy and is omitted from save export

### Requirement: Delete is soft-delete with stable EntityId
Editor delete SHALL soft-delete the target entity: keep its EntityId, remove it from the editable view, and disable or tombstone it. Undo SHALL restore visibility/enabled state. Delete SHALL NOT erase the entity from the middle of the dense entity storage in a way that shifts other EntityIds.

#### Scenario: Delete then undo
- **WHEN** the user deletes an entity and then undoes
- **THEN** the same EntityId refers to the restored entity with its prior editable state

### Requirement: Save omits tombstoned entities
Save and `exportToScene` SHALL omit soft-deleted (tombstoned) entities so the on-disk scene matches the editable document. Tombstones MAY remain in the live SceneInstance only to support in-session undo.

#### Scenario: Save after delete
- **WHEN** the user soft-deletes an entity and saves the active scene
- **THEN** the written scene asset does not contain that entity

### Requirement: Commands target EntityId in v1
MVP Editor Commands SHALL address entities by EntityId within the active SceneInstance. ObjectId SHALL NOT be required as the command target for this milestone.

#### Scenario: Transform command uses EntityId
- **WHEN** a transform Command is recorded
- **THEN** it identifies the affected entity by EntityId

### Requirement: Soft-deleted entities omit Behaviours on save
Save and export SHALL omit soft-deleted entities entirely, including any Behaviour declarations they had while tombstoned in the live session.

#### Scenario: Tombstone drops behaviours from disk
- **WHEN** a user soft-deletes an entity that had Behaviours and saves
- **THEN** the written scene asset contains neither that entity nor its Behaviours

### Requirement: Placement Preview does not spawn
A visible Placement Preview SHALL NOT create a scene Entity and SHALL NOT push a Spawn Editor Command. Spawn remains sealed only on a successful Mesh Asset drop onto the viewport.

#### Scenario: Preview motion pushes no history
- **WHEN** the user drags a Mesh Asset over the viewport so Placement Preview follows the pointer and then cancels without dropping
- **THEN** Document History has no new Spawn Entity Command

### Requirement: Add… and Remove attachment are Editor Commands
Successfully applying Add… (including host cascade and Skeleton hydration), Remove attachment, Add clip, or clip-row Remove SHALL push a single Editor Command on Document History for the active scene. Commands SHALL target EntityId. One author click SHALL be one Command: cascaded hosts and hydration SHALL undo and redo together, not as separate history steps.

#### Scenario: Undo Add AnimationPlayer cascade
- **WHEN** the author adds AnimationPlayer (creating Skeleton and hydrating bones) and then undoes
- **THEN** AnimationPlayer, the created Skeleton, and hydrated bones are gone, and Document History does not leave a leftover Skeleton-only step

#### Scenario: Undo Add Camera
- **WHEN** the author adds Camera from Add… and then undoes
- **THEN** that entity has no Camera Component

#### Scenario: Redo Add clip
- **WHEN** the author Add clips a row, undoes, then redoes
- **THEN** the empty name→GUID row is restored on that entity’s AnimationPlayer

### Requirement: Create… is one Document History Command
Successfully applying a Hierarchy **Create…** item (Empty, Camera, or Light) SHALL push a single Editor Command on Document History for the active scene. That Command SHALL include spawning the entity, parenting, identity local TRS, the unique name, optional Unique attachment (Camera or Light), and the post-Create selection. Undo SHALL remove that entity from the editable document and restore the previous selection. Redo SHALL restore the entity with the same EntityId and Unique attachment. Create… SHALL NOT be a Spawn Entity Command followed by a separate Add… command, and SHALL NOT be recorded on Global History.

#### Scenario: Undo Create Light
- **WHEN** the author Create… Light and then undoes
- **THEN** the new Light entity is not visible in the editable hierarchy, Document History does not leave a leftover Unique-only step, and selection matches the pre-Create snapshot

#### Scenario: Redo Create Camera
- **WHEN** the author Create… Camera, undoes, then redoes
- **THEN** the same EntityId is restored with a Camera Component that is not Main

#### Scenario: Create Empty is one undo
- **WHEN** the author Create… Empty and then undoes
- **THEN** that entity is gone and no second undo step remains for the spawn

### Requirement: Soft-delete Command label is Delete name
Successfully applying editor scene-entity delete (Hierarchy Delete or the Delete key) SHALL push one Document History Command whose Command label is `Delete {name}` using the entity name snapshotted when the command is pushed. When no name is available the label SHALL be `Delete Entity`. The menu path and the Delete key SHALL use the same command type. The label SHALL NOT remain the default `Edit` when the entity name is known.

#### Scenario: History shows Delete Cube
- **WHEN** the author deletes an entity named `Cube` from the Hierarchy row menu or with the Delete key
- **THEN** Document History shows `Delete Cube` as that command’s label

#### Scenario: Unknown name falls back
- **WHEN** the author deletes an entity whose name is empty
- **THEN** Document History shows `Delete Entity`

### Requirement: Light Add Remove and property Commands
Successfully adding Light, removing Light, or committing Inspector Light Component fields (type, color, intensity, enabled, contribution, range, cone, Area size, linking list) SHALL push Document History Commands targeted by EntityId. Undo of Add Light SHALL leave the entity with no Light Component.

#### Scenario: Undo Add Light
- **WHEN** the author adds Light from Add… and then undoes
- **THEN** that entity has no Light Component

#### Scenario: Undo Light type change
- **WHEN** the author changes a Light Component from Directional to Point and then undoes
- **THEN** the Light Component type is Directional again

