# document-history Specification

## Purpose
Editor Document History: linear per-scene command stack, dirty baseline vs save, selection restore on undo/redo, and shared Undo/Redo input (shortcuts + Edit UI).

## Requirements

### Requirement: Document History owns one linear stack per active scene
The editor SHALL maintain a Document History for the active editable scene. Opening a different scene SHALL clear or replace that history. History SHALL NOT span unrelated documents or survive unload of the active scene instance as a continuous undo continuum.

#### Scenario: Open scene clears history
- **WHEN** the user opens a scene via `openScene` (or equivalent)
- **THEN** Document History cannot undo edits that belonged to the previous active document

#### Scenario: Undo after edit
- **WHEN** an Editor Command has been executed on the active document
- **THEN** Document History can undo that Command and restore the pre-command document state for that Command’s effects

### Requirement: Linear stack truncates redo on new edit
Document History SHALL be a single linear stack with a cursor. After one or more undos, executing a new Editor Command SHALL discard the redo tail beyond the cursor.

#### Scenario: Divergent edit after undo
- **WHEN** the user undoes at least one Command and then executes a new Editor Command
- **THEN** previously undone Commands SHALL NOT be redoable

### Requirement: Fixed stack depth limit
Document History SHALL enforce a fixed maximum number of stored Commands (default 100). Pushing beyond the limit SHALL drop the oldest Command.

#### Scenario: Overflow drops oldest
- **WHEN** the stack is at the configured maximum and a new Command is executed
- **THEN** the oldest Command is removed and the new Command is retained

### Requirement: Dirty tracks save baseline cursor
Document dirty SHALL reflect divergence from the last successful save. Saving SHALL record a history baseline cursor. Undo/redo that returns the cursor to that baseline SHALL clear dirty; otherwise dirty SHALL remain set. Explicit successful Save SHALL clear dirty and refresh the baseline.

#### Scenario: Undo to saved state
- **WHEN** the document was saved and the user undoes back to the save baseline
- **THEN** the document is not dirty

#### Scenario: Edit after save
- **WHEN** the user executes a Command after a successful save
- **THEN** the document is dirty

### Requirement: Commands restore selection snapshots
Each Editor Command SHALL store before and after selection snapshots. Undo SHALL restore the before selection; redo SHALL restore the after selection.

#### Scenario: Undo restores prior selection
- **WHEN** the user undoes a Command that changed selection as part of its after snapshot
- **THEN** selection matches the Command’s before snapshot

### Requirement: Undo and Redo user input
The editor SHALL expose Undo and Redo through Ctrl+Z, Redo via Ctrl+Y and Ctrl+Shift+Z, and Edit menu items. Menu and shortcuts SHALL call the same Document History API. UI affordances SHALL enable based on canUndo/canRedo.

#### Scenario: Ctrl+Z undoes
- **WHEN** Document History can undo and the user presses Ctrl+Z in the editor (when the shortcut is not claimed by an exclusive text-undo context)
- **THEN** the last Command is undone
