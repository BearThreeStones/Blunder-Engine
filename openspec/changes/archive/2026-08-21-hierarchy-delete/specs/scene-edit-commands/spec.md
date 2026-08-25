## ADDED Requirements

### Requirement: Soft-delete Command label is Delete name
Successfully applying editor scene-entity delete (Hierarchy Delete or the Delete key) SHALL push one Document History Command whose Command label is `Delete {name}` using the entity name snapshotted when the command is pushed. When no name is available the label SHALL be `Delete Entity`. The menu path and the Delete key SHALL use the same command type. The label SHALL NOT remain the default `Edit` when the entity name is known.

#### Scenario: History shows Delete Cube
- **WHEN** the author deletes an entity named `Cube` from the Hierarchy row menu or with the Delete key
- **THEN** Document History shows `Delete Cube` as that command’s label

#### Scenario: Unknown name falls back
- **WHEN** the author deletes an entity whose name is empty
- **THEN** Document History shows `Delete Entity`
