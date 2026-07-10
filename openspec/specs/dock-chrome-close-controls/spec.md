# dock-chrome-close-controls Specification

## Purpose
TBD - created by archiving change dock-browser-tab-chrome. Update Purpose after archive.
## Requirements
### Requirement: Window close control on integrated chrome

Native OS floating windows and in-host floating frames SHALL expose a dedicated window-close control at the top-right of the integrated chrome row, separate from per-tab close buttons.

#### Scenario: Window close on OS float

- **WHEN** the user activates the top-right window-close control on an OS float chrome row
- **THEN** the system SHALL close the float via the existing float close path (`close-pressed` / `closeWidget` for the floated content)

#### Scenario: Window close visible with multiple tabs

- **WHEN** an OS float chrome row shows more than one tab
- **THEN** exactly one window-close control SHALL remain at the top-right of the chrome row
- **AND** each tab SHALL retain its own tab-close control

### Requirement: Per-tab close closes widget only

Per-tab close buttons (×) on the integrated chrome row SHALL close only that dock widget, not sibling tabs in the same container, using the existing `widget-closed` / `closeWidget` behavior.

#### Scenario: Tab close removes one widget

- **WHEN** the user activates the × on a specific tab in a multi-tab chrome row
- **THEN** only that widget SHALL be removed from the dock tree
- **AND** remaining tabs in the same container SHALL stay visible

#### Scenario: Tab close distinct from window close

- **WHEN** the user activates the tab × on the last remaining tab in an OS float
- **THEN** the widget SHALL close per `closeWidget` rules
- **AND** the window-close control SHALL remain the separate top-right affordance (not merged into tab ×)

