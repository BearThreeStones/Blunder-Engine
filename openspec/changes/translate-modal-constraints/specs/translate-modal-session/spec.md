## ADDED Requirements

### Requirement: Mid-session keyboard axis and plane constraints

While a Translate Modal Session is active (handle or grab entry), the editor SHALL accept `X` / `Y` / `Z` to set a single-axis constraint and `Shift+X` / `Shift+Y` / `Shift+Z` to set a plane constraint that locks out that axis (YZ / ZX / XY). Re-pressing the same constraint key SHALL cycle orientation global → local → free (constraint cleared). Pressing a different constraint key SHALL apply that constraint in global orientation. After a constraint change, motion SHALL be recomputed from session-start transform and the current pointer (or active numeric value).

#### Scenario: X constrains to global X

- **WHEN** a Translate Modal Session is active with free or other constraint
- **AND** the user presses `X`
- **THEN** the session uses a single-axis X constraint in global orientation
- **AND** further pointer motion moves the entity only along world X

#### Scenario: Re-press X cycles to local then free

- **WHEN** a Translate Modal Session is constrained to global X
- **AND** the user presses `X`
- **THEN** the session uses a single-axis X constraint in local orientation
- **AND** when the user presses `X` again
- **THEN** the constraint clears to free view-plane translation

#### Scenario: Shift+Z constrains to XY plane

- **WHEN** a Translate Modal Session is active
- **AND** the user presses `Shift+Z`
- **THEN** the session uses an XY plane constraint
- **AND** pointer motion keeps the entity in that plane

#### Scenario: Constraint change works during grab

- **WHEN** a grab-started Translate Modal Session is active
- **AND** the user presses `Y`
- **THEN** the session switches to a global Y axis constraint
- **AND** constraint guides for Y become visible

### Requirement: MMB axis pick during translate session

While a Translate Modal Session is active, middle-mouse press-drag-release SHALL pick a single-axis constraint by choosing the nearest projected axis to the mouse delta (Blender-like). On MMB release the session SHALL continue under that axis constraint. Camera orbit MUST NOT run during the pick.

#### Scenario: MMB drag picks nearest axis

- **WHEN** a Translate Modal Session is active
- **AND** the user presses MMB, moves toward the screen projection of world Y, and releases MMB
- **THEN** the session uses a single-axis Y constraint
- **AND** the camera does not orbit

#### Scenario: MMB pick ignored when session inactive

- **WHEN** no Translate Modal Session is active
- **AND** the user middle-drags in the viewport
- **THEN** this requirement does not consume the gesture for axis pick

### Requirement: Numeric distance input during translate session

While a Translate Modal Session is active, typing a number (digits with optional leading `-` and one `.`) SHALL enter numeric input mode and set the translation distance along the active constraint. While numeric input is active, pointer motion MUST NOT override the numeric value. Backspace SHALL edit the buffer; clearing the buffer SHALL exit numeric mode and restore pointer-driven motion. Escape SHALL clear numeric input first when numeric mode is active, and only cancel the session when numeric mode is inactive. Confirm SHALL keep the numeric result.

#### Scenario: Digits set exact axis distance

- **WHEN** a Translate Modal Session is constrained to global X
- **AND** the user types `2`
- **AND** the user confirms the session
- **THEN** the entity translation along world X equals 2 (in engine units from session start)

#### Scenario: Escape clears numeric before cancel

- **WHEN** a Translate Modal Session is active with numeric input `1.5`
- **AND** the user presses Escape
- **THEN** numeric input clears and the session remains active
- **AND** when the user presses Escape again
- **THEN** the session cancels and restores session-start transform

#### Scenario: Pointer ignored during numeric

- **WHEN** numeric input is active on a Translate Modal Session
- **AND** the user moves the pointer
- **THEN** the entity position continues to follow the numeric value, not the pointer delta

### Requirement: Feedback follows live constraint

When the active constraint of a Translate Modal Session changes (keyboard or MMB), constraint guides, origin dot, and plane/center visibility SHALL update to match the live constraint using the same rules as handle-started constrained sessions. Grab entry SHALL still draw no handle drag ghost. Free constraint SHALL draw no guides and no origin dot.

#### Scenario: Grab gains guides when constrained

- **WHEN** a grab-started session is free
- **AND** the user presses `X`
- **THEN** one X-colored constraint guide is drawn through the drag-start pivot
- **AND** no handle drag ghost is drawn

#### Scenario: Clearing constraint removes guides

- **WHEN** a session is axis-constrained with guides visible
- **AND** the user cycles the same axis key to free
- **THEN** no constraint guides are drawn

### Requirement: Enter confirms translate modal session

While a Translate Modal Session is active, pressing Enter SHALL confirm the session and keep the current entity transform, for both handle and grab entry.

#### Scenario: Enter confirms grab

- **WHEN** a grab-started Translate Modal Session is active
- **AND** the user presses Enter
- **THEN** the session ends
- **AND** the entity remains at the current transform
