## MODIFIED Requirements

### Requirement: Undo and Redo user input
The editor SHALL expose Undo and Redo through Ctrl+Z, Redo via Ctrl+Y and Ctrl+Shift+Z, and Edit menu items. When the Content Browser panel has input focus, or when the Inspector panel has input focus and presentation is Asset Inspector, those shortcuts and the Edit menu SHALL call Global History. Otherwise they SHALL call Document History. UI affordances SHALL enable based on canUndo/canRedo of the routed stack. Exclusive text-undo contexts (including Inline Rename) SHALL claim the shortcut while active. This routing SHALL NOT merge Scene and Global into one timeline.

#### Scenario: Ctrl+Z undoes
- **WHEN** Document History can undo and the user presses Ctrl+Z in the editor (when the shortcut is not claimed by an exclusive text-undo context) and neither Content Browser nor Asset Inspector has input focus
- **THEN** the last Document History Command is undone

#### Scenario: Asset Inspector focus undoes Global History
- **WHEN** Global History can undo and Inspector has input focus in Asset Inspector
- **AND** the user presses Ctrl+Z
- **THEN** the last Global Command is undone
- **AND** Document History is unchanged
