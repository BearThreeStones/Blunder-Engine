## ADDED Requirements

### Requirement: Editor Commands expose English labels
Each Editor Command on Document History SHALL expose an English display label for the History Panel. For MVP commands that target an entity, the label SHALL include the entity display name snapshotted at push time. When no name is available, the label SHALL fall back to a type-only English phrase (`Set Transform`, `Spawn Entity`, `Delete Entity`).

#### Scenario: Transform command label includes name
- **WHEN** a SetEntityTransform command is pushed for an entity whose display name is "Player"
- **THEN** the command label includes "Player" (e.g. `Move Player`)

#### Scenario: Rename does not rewrite label
- **WHEN** a command was pushed with snapshotted name "Cube" and the entity is renamed afterward
- **THEN** querying that command's label still returns the snapshotted "Cube" form

### Requirement: Document History supports list and seek
Document History SHALL expose enough read API for the History Panel to list labels in stack order and to seek the cursor to a chosen index by applying undo/redo. Seek SHALL use the same undo/redo path as shortcuts.

#### Scenario: Seek to earlier entry
- **WHEN** Document History has three applied commands and the panel seeks to the first
- **THEN** two undos occur and canUndo reflects the new cursor

#### Scenario: Seek into redo tail
- **WHEN** Document History has a redo tail and the panel seeks to a redo entry
- **THEN** redo is applied until that entry is current
