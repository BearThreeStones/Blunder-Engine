## MODIFIED Requirements

### Requirement: Tab drag drives float and re-dock

Dragging a tab in the integrated chrome row SHALL be the primary gesture to detach, show drag preview, commit OS float, and re-dock with dock guides. A press-and-release on a tab without pointer movement beyond the configured tab-drag threshold SHALL activate the tab only and SHALL NOT start a committed dock operation (split, float, or auto-hide). Drag preview and guide hover SHALL begin only after the pointer exceeds that threshold.

#### Scenario: Tab drag from OS float starts dock drag

- **WHEN** the user presses and drags a tab in the OS float chrome row beyond the tab-drag threshold
- **THEN** the system SHALL call the same tab-drag path as today (`beginDrag`, global pointer poll when applicable)
- **AND** re-dock guides SHALL appear when hovering valid main-window dock targets

#### Scenario: Tab drag from main window docked panel

- **WHEN** the user presses and drags a tab in a main-window dock container chrome row beyond the tab-drag threshold
- **THEN** the system SHALL drive drag preview and guide hover without requiring a separate title bar row

#### Scenario: Tab click without drag does not commit dock change

- **WHEN** the user presses and releases a tab without moving the pointer beyond the tab-drag threshold
- **THEN** the system SHALL activate the tab widget
- **AND** SHALL NOT commit split dock, OS float, or auto-hide on release
