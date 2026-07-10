# dock-browser-tab-chrome Specification

## Purpose
TBD - created by archiving change dock-browser-tab-chrome. Update Purpose after archive.
## Requirements
### Requirement: Integrated browser-style dock chrome row

The editor SHALL render dock panels and native OS floating panels with a single top chrome row where tabs are anchored at the top-left in a browser-style strip, replacing the separate title row stacked above a tab row on OS floats.

#### Scenario: OS float shows one chrome row

- **WHEN** a panel is displayed in a native OS floating window
- **THEN** the window SHALL show exactly one top chrome row containing tabs at the top-left
- **AND** the window SHALL NOT show a separate title-text row above the tab row

#### Scenario: Main window dock container uses same chrome model

- **WHEN** a panel is docked in the main editor docking host
- **THEN** the container SHALL show tabs in the same integrated chrome row at the top of the container’s `tab_bar_rect`
- **AND** the chrome visual style SHALL match native OS floats (tab shape, height, colors)

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

### Requirement: Chrome blank area moves OS float window

On native OS floating windows, dragging the empty chrome strip to the right of the tab strip SHALL move the entire float window; dragging a tab SHALL NOT move the window.

#### Scenario: Drag chrome strip moves float

- **WHEN** the user drags the chrome blank area (not on a tab) on an OS float
- **THEN** the float window position SHALL update via the existing floating-move interaction

#### Scenario: Drag tab does not move float window

- **WHEN** the user drags a tab on an OS float
- **THEN** the system SHALL start tab drag
- **AND** SHALL NOT start floating-window move from that gesture

### Requirement: Unified chrome height for resize hit-testing

Resize edge hit-testing on native OS floats SHALL treat the integrated chrome row as a single top inset whose height matches the Slint `chrome_bar_height` (not `title_height + tab_bar_height`).

#### Scenario: Resize cursor below chrome row

- **WHEN** the pointer is over a content-area edge below the integrated chrome row on an OS float
- **THEN** the system SHALL offer resize cursors and resize interaction as today

#### Scenario: No resize cursor on chrome row

- **WHEN** the pointer is over the integrated chrome row on an OS float
- **THEN** the system SHALL NOT show content-edge resize cursors

