## ADDED Requirements

### Requirement: No duplicate panel title in content area

When a dock panel displays its name on an integrated chrome tab, the panel content area SHALL NOT repeat the same panel title as a separate in-content header label.

#### Scenario: Docked Hierarchy without duplicate header

- **WHEN** the Hierarchy panel is docked in the main window with a visible "Hierarchy" tab in the chrome row
- **THEN** the content area SHALL NOT show a second standalone "Hierarchy" title label at the top of the panel body

#### Scenario: OS float Hierarchy without duplicate header

- **WHEN** the Hierarchy panel is shown in a native OS float with a "Hierarchy" tab in the chrome row
- **THEN** the float content area SHALL NOT show a redundant "Hierarchy" header above the tree view

### Requirement: Functional panel UI preserved

Removing duplicate title labels SHALL NOT remove functional controls inside panels (search fields, toolbars, inspector fields, browser path bar).

#### Scenario: Content Browser keeps functional chrome

- **WHEN** the Content Browser panel is shown with a chrome tab
- **THEN** path segments, search, and grid/tree controls SHALL remain available in the content area
- **AND** only the redundant static panel-name label SHALL be removed
