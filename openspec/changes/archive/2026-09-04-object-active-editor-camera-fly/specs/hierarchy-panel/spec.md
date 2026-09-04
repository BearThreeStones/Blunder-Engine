## ADDED Requirements

### Requirement: Hierarchy Active checkbox
Each Hierarchy entity row SHALL show a checkbox for that Object's Object Active. The row name SHALL use muted (grey) text when the Object is not Active in Hierarchy. Left pointer on the checkbox SHALL NOT toggle expand/collapse and SHALL NOT start Inline Rename. When the row is in the selection, the checkbox SHALL use the same align rule as Hierarchy Active toggle. When the row is not in the selection, the checkbox SHALL select that entity (single) and toggle that Object's Object Active.

#### Scenario: Checkbox shows local flag while name is grey
- **WHEN** a parent is Object Active off and a child is Object Active on
- **THEN** the child's checkbox is on
- **AND** the child's name is grey

#### Scenario: Checkbox does not expand
- **WHEN** the author presses the left pointer on the Active checkbox of a collapsed parent row
- **THEN** Object Active changes
- **AND** the row does not expand from that press

### Requirement: Hierarchy Active toggle key
While the pointer is over the Hierarchy Panel (docked or floating), A is not used by Inline Rename, and a selection exists, A SHALL run Hierarchy Active toggle. A SHALL NOT toggle Object Active when the pointer is over the viewport, Inspector, or Content Browser.

#### Scenario: A over Hierarchy toggles
- **WHEN** the pointer is over the Hierarchy Panel
- **AND** one Object is selected
- **AND** Inline Rename is not active
- **AND** the author presses A
- **THEN** that Object's Object Active flips

#### Scenario: A over viewport does not toggle
- **WHEN** the pointer is over the editor viewport
- **AND** an Object is selected
- **AND** the author presses A without right or middle mouse held
- **THEN** Object Active is unchanged
