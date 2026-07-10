## ADDED Requirements

### Requirement: Session-active input routing for constraints

While a Translate Modal Session is active, the transform gizmo / editor 3D shortcut path SHALL route `X` / `Y` / `Z` (with optional Shift), numeric typing keys, Backspace, Enter, and MMB axis-pick gestures into the session. These inputs MUST NOT start a different gizmo mode, start camera orbit, or begin a new handle pick. Idle mode shortcuts such as `G` / `R` / `S` MUST NOT replace the active session.

#### Scenario: X during session does not leave translate modal

- **WHEN** a Translate Modal Session is active
- **AND** the user presses `X`
- **THEN** the session remains active with an updated constraint
- **AND** the gizmo mode is not switched away by that key

#### Scenario: MMB during session does not orbit

- **WHEN** a Translate Modal Session is active
- **AND** the user middle-drags
- **THEN** the gesture is consumed for axis pick (or ignored only if not eligible)
- **AND** the editor camera does not orbit

#### Scenario: Digit during session starts numeric input

- **WHEN** a Translate Modal Session is active and editor 3D shortcuts are enabled
- **AND** the user presses a digit key
- **THEN** the session enters numeric input mode
- **AND** the key is not handled as an unrelated editor shortcut
