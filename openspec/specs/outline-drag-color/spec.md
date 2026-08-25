# outline-drag-color

## Purpose

Translate Modal Session (handle drag) drives a light transform-active outline; rotate and scale handle drags still use transform-drag orange until those modes gain a modal session.

## Requirements

### Requirement: Transform-drag outline color

While a Translate Modal Session is active from a translate handle drag, the outline resolve pass SHALL composite pixels using a fixed light/off-white **transform-active** outline color instead of the object-select orange. When the session ends (confirm or cancel), the outline color SHALL return to the object-select orange (`#F57011`). Rotate and scale handle drags MAY continue to use the previous transform-drag orange until those modes gain a modal session.

#### Scenario: Begin translate handle drag

- **WHEN** the user starts dragging a translate gizmo handle on a selected mesh entity
- **THEN** the viewport outline color SHALL switch to the fixed light transform-active color for the duration of the Translate Modal Session

#### Scenario: End translate handle drag

- **WHEN** the user confirms or cancels the translate handle session
- **THEN** the viewport outline color SHALL return to the object-select orange (`#F57011`)

#### Scenario: Begin rotate or scale gizmo drag

- **WHEN** the user starts dragging a rotate or scale gizmo handle on a selected mesh entity
- **THEN** the viewport outline color SHALL switch to the hardcoded transform orange for the duration of the drag

#### Scenario: End rotate or scale gizmo drag

- **WHEN** the user releases the mouse button and the rotate or scale gizmo drag ends
- **THEN** the viewport outline color SHALL return to the object-select orange (`#F57011`)

#### Scenario: No selection during drag

- **WHEN** selection is cleared while a translate modal session is active or a gizmo drag is active
- **THEN** the outline pass SHALL be disabled and no outline pixels SHALL be drawn
