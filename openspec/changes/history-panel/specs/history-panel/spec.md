## ADDED Requirements

### Requirement: History Panel sibling to Content Browser
The editor SHALL provide a History Panel as a sibling tab to the Content Browser (filesystem) in the same tab group. The panel SHALL list filtered Editor History entries and accept clicks for History Jump.

#### Scenario: History tab available beside filesystem
- **WHEN** the user opens the dock group that contains Content Browser
- **THEN** a History tab is available in that same tab group

#### Scenario: Click jumps history
- **WHEN** Document History has commands and the user clicks a listed Scene entry
- **THEN** Document History seeks to that entry via undo/redo until the cursor matches the entry

### Requirement: History scope filter
The History Panel SHALL provide Scene and Global checkboxes. Default SHALL be both checked. Scene SHALL show Document History. Global SHALL show Global History. When both are checked in this milestone, the panel SHALL show only Document History (no merge). When both are unchecked, the list SHALL be empty.

#### Scenario: Default filters
- **WHEN** the History Panel is first shown in a session
- **THEN** Scene and Global are both checked and the list shows Document History entries (if any)

#### Scenario: Both unchecked empties list
- **WHEN** the user unchecks Scene and Global
- **THEN** the History Panel list is empty regardless of stack contents

#### Scenario: Global alone while empty
- **WHEN** Global is checked, Scene is unchecked, and Global History has no commands
- **THEN** the History Panel list is empty

### Requirement: Command labels and row presentation
Each listed Document History entry SHALL show an English Command label. Labels that include an entity name SHALL use the name snapshotted when the command was pushed. The list SHALL be ordered oldest-at-top, newest-at-bottom. Entries in the redo tail (after the stack cursor) SHALL be visually muted. The current cursor position SHALL be highlighted. Muted rows SHALL remain clickable.

#### Scenario: Label snapshots entity name
- **WHEN** a Move command is pushed for an entity named "Cube" and the entity is later renamed to "Hero"
- **THEN** that history row still displays a label referring to "Cube" (not "Hero")

#### Scenario: Redo tail is muted
- **WHEN** the user undoes one or more commands so a redo tail exists
- **THEN** redo-tail rows are visually distinct from applied rows and remain clickable for History Jump

### Requirement: Shortcuts stay on Document History
Editor Undo/Redo shortcuts and Edit menu items SHALL continue to target Document History only. They SHALL NOT route to Global History in this milestone.

#### Scenario: Ctrl+Z ignores empty Global
- **WHEN** Global History is empty and Document History can undo
- **THEN** Ctrl+Z undoes the Document History command
