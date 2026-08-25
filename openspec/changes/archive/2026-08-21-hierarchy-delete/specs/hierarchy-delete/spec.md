## Purpose

Lets authors delete a Hierarchy entity from the same row right-click menu as Create…, using the same scene-entity operation as the Delete key.

## ADDED Requirements

### Requirement: Hierarchy Delete is hosted on entity rows only
Hierarchy **Delete** SHALL be available on the Hierarchy Panel when docked and when floating. It SHALL appear only on a visible entity-row right-click menu. Empty Hierarchy area and the scene title chrome SHALL NOT offer Delete. This slice SHALL NOT add Delete to the viewport or the editor top bar.

#### Scenario: Floating Hierarchy Delete
- **WHEN** Hierarchy is in a floating window and the author chooses Delete on an entity row
- **THEN** that entity is scene-deleted the same as from the docked Hierarchy Panel

#### Scenario: Empty area has no Delete
- **WHEN** the author right-clicks empty Hierarchy area
- **THEN** the menu lists Empty, Camera, and Light and does not list Delete or a trailing separator

#### Scenario: Scene title has no Delete
- **WHEN** the author right-clicks the scene display name chrome
- **THEN** the menu lists Empty, Camera, and Light and does not list Delete or a trailing separator

### Requirement: Row menu lists Delete after a separator
On a visible entity row the right-click menu SHALL be a flat list: `Empty`, `Camera`, `Light`, a separator, then `Delete`. The Delete item title SHALL be English `Delete`. This slice SHALL NOT add Duplicate or Rename.

#### Scenario: Row menu shows Create items then Delete
- **WHEN** the author right-clicks a Hierarchy entity row
- **THEN** the menu lists Empty, Camera, Light, a separator, and Delete, and does not list Duplicate or Rename

### Requirement: Hierarchy Delete matches the Delete key
Hierarchy Delete SHALL be the same scene-entity operation as the Delete key: one Document History Command, soft-delete of the right-clicked entity only (stable EntityId / tombstone). Descendants SHALL remain parented; they SHALL leave the editable document while an ancestor is tombstoned and SHALL return when that entity is undone. Hierarchy Delete SHALL NOT tombstone each descendant as its own Command. Hierarchy Delete SHALL NOT be a Global Command. Choosing Delete with no active scene SHALL be a no-op.

#### Scenario: One undo restores the entity and its children
- **WHEN** the author Hierarchy-Deletes a parent that has children and then undoes
- **THEN** that parent is visible again with the same EntityId, the children are visible again still parented to it, and Document History does not leave leftover per-child delete steps

#### Scenario: Child is omitted while the parent is tombstoned
- **WHEN** the author Hierarchy-Deletes a parent that has a child
- **THEN** the parent is not visible in the editable hierarchy and the child is not visible either, and the child is not a separate History row

### Requirement: No confirm and no Main Camera protection
Hierarchy Delete SHALL NOT show a confirm dialog. Create… Camera and **Main Camera** SHALL NOT be protected from delete.

#### Scenario: Delete Main Camera
- **WHEN** the author chooses Delete on the Main Camera row
- **THEN** that entity is soft-deleted with no confirm dialog

### Requirement: Selection is cleared after delete
After a successful Hierarchy Delete, selection SHALL be cleared, matching the Delete key.

#### Scenario: Inspector has no entity after Delete
- **WHEN** the author Hierarchy-Deletes the selected entity
- **THEN** there is no selected entity
