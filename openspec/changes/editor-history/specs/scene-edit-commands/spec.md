## ADDED Requirements

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
