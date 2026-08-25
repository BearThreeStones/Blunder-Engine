## MODIFIED Requirements

### Requirement: Undo and Redo user input
The editor SHALL expose Undo and Redo through Ctrl+Z, Redo via Ctrl+Y and Ctrl+Shift+Z, and Edit menu items. When the Content Browser panel has input focus, those shortcuts and the Edit menu SHALL call Global History. Otherwise they SHALL call Document History. UI affordances SHALL enable based on canUndo/canRedo of the routed stack. Exclusive text-undo contexts (including Inline Rename) SHALL claim the shortcut while active. This routing SHALL NOT merge Scene and Global into one timeline.

#### Scenario: Ctrl+Z undoes
- **WHEN** Document History can undo and the user presses Ctrl+Z in the editor (when the shortcut is not claimed by an exclusive text-undo context) and Content Browser does not have input focus
- **THEN** the last Document History Command is undone

#### Scenario: Browser focus undoes Global History
- **WHEN** Global History can undo and the Content Browser has input focus and Inline Rename is not active
- **AND** the user presses Ctrl+Z
- **THEN** the last Global Command is undone
- **AND** Document History is unchanged
