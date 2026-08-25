## ADDED Requirements

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
