# dock-float-resize-cursor Specification

## Purpose
TBD - created by archiving change dock-float-redock-guide-stability. Update Purpose after archive.
## Requirements
### Requirement: OS resize cursor on native float edges

When the pointer hovers over a resize edge or corner grip on a native OS floating window, the system SHALL set the appropriate OS cursor (north/south/east/west and diagonal variants) matching `DockResizeEdge`. The default arrow cursor SHALL restore when the pointer leaves resize zones.

#### Scenario: East edge shows east-west cursor

- **WHEN** the pointer hovers over the east resize strip of a native floating window
- **THEN** the OS cursor SHALL indicate horizontal resize
- **AND** dragging with the left button SHALL resize the window width

#### Scenario: Southeast grip shows diagonal cursor

- **WHEN** the pointer hovers over the southeast resize grip
- **THEN** the OS cursor SHALL indicate southeast diagonal resize

#### Scenario: Cursor restores outside resize zones

- **WHEN** the pointer moves from a resize edge to the panel content area
- **THEN** the OS cursor SHALL return to the default arrow (or title-bar move cursor over the title bar)

### Requirement: Optional Slint edge hover feedback

Native floating window Slint chrome SHALL support a subtle hover tint on resize edge strips independent of OS cursor propagation, consistent with the existing southeast grip hover color.

#### Scenario: Edge strip hover tint

- **WHEN** the pointer hovers a resize edge TouchArea in `floating_panel_window.slint`
- **THEN** the edge strip SHALL display a visible hover background distinct from the default chrome

