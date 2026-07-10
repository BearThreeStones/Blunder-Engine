# dock-host-edge-guides Specification

## Purpose

Host-level edge dock guides during tab drag: four perimeter icons with proximity fade-in, hover highlight, half-window preview, and root-level `dockToRoot` drop targets.
## Requirements
### Requirement: Host edge guide icons on all four edges

During an active dock tab drag, the layout model SHALL expose four host-level edge dock guide icons centered on the top, bottom, left, and right edges of `m_host_rect`.

#### Scenario: Four host guides during drag

- **WHEN** the user drags any dock tab over the dock host
- **THEN** the system SHALL be capable of rendering host edge guide icons on all four host edges
- **AND** each icon SHALL be positioned at the center of its edge (not tied to a container rect)

### Requirement: Host guide proximity fade-in

When the pointer is within the configured host guide proximity distance of any host edge during drag, all four host edge guide icons SHALL be visible at reduced (faint) opacity.

#### Scenario: Approach host edge shows faint guides

- **WHEN** the user drags a tab within 48 logical pixels of the right host edge
- **THEN** all four host edge guide icons SHALL be emitted with faint opacity
- **AND** icons not under the cursor SHALL remain faint until the pointer leaves proximity or the drag ends

#### Scenario: Far from edges hides host guides

- **WHEN** the pointer is in the center of the dock host and farther than the proximity threshold from every host edge
- **THEN** host edge guide icons SHALL NOT be visible

### Requirement: Host guide hover highlight and half-window preview

When the pointer hovers a host edge guide icon, that icon SHALL highlight and the system SHALL show a half-window dock preview on the corresponding host edge.

#### Scenario: Hover right host guide

- **WHEN** the pointer is over the right host edge guide icon during tab drag
- **THEN** that guide SHALL be highlighted (bolder frame)
- **AND** a semi-transparent preview SHALL cover approximately half of the dock host width on the right side
- **AND** the preview SHALL span the full host height

#### Scenario: Non-hovered host guides stay faint

- **WHEN** the pointer highlights the right host guide icon
- **THEN** the other three host guide icons SHALL remain at faint opacity without half-window previews

### Requirement: Host guide icon-only hit target

Host edge dock drop detection SHALL use the full host guide component rectangle (arrow plus icon body). Full-edge bands SHALL NOT activate host edge dock.

#### Scenario: Pointer on icon activates host dock target

- **WHEN** the pointer is inside the right host guide component bounds (including arrow and body)
- **THEN** the drag controller SHALL set the hovered host dock slot to right
- **AND** SHALL NOT require the pointer to be in a full-height band along the host right edge

#### Scenario: Pointer in edge zone without icon does not host-dock

- **WHEN** the pointer is within the auto-hide edge zone on the right but not over the host guide component bounds
- **THEN** the system SHALL NOT treat the hover as a host edge dock target
- **AND** MAY treat it as an auto-hide edge target if pinnable

### Requirement: Drop on host guide docks to root

Releasing the drag while a host edge guide is hovered SHALL dock the dragged widget to the corresponding side of the entire workspace via `dockToRoot`.

#### Scenario: Drop on right host guide

- **WHEN** the user releases a tab drag while hovering the right host edge guide icon
- **THEN** the dragged widget SHALL be docked to the right of the root layout occupying approximately half the workspace
- **AND** the main layout SHALL split at the host level (not inside a single container only)

#### Scenario: Drop on left host guide

- **WHEN** the user releases while hovering the left host edge guide icon
- **THEN** the dragged widget SHALL be docked to the left of the root layout at half-window scale
- **AND** existing panels SHALL reflow to the remaining half

### Requirement: Host guide visual style

Host edge guide icons SHALL use the shared flat-wide `DockEdgeGuideButton` pictograph (white inward arrow, blue chrome bar, full light-blue body). They SHALL NOT use the square `DockGuideButton` half-pane style with dashed dividers used by container split cross guides.

#### Scenario: Host guide uses flat-wide arrow icon

- **WHEN** host edge guide icons are visible during drag
- **THEN** they SHALL render via `DockEdgeGuideButton` with orientation per host edge
- **AND** they SHALL include a white inward arrow on the outer side of the host rect

#### Scenario: Host guide distinct from container cross

- **WHEN** host edge guide icons and container cross guides are visible during the same drag
- **THEN** host guides SHALL use the flat-wide arrow style
- **AND** container cross guides SHALL continue to use `DockGuideButton` half-pane style with dashed dividers

