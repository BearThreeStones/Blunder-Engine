# dock-guide-hit-layers Specification

## Purpose

Three-layer dock drag hit resolution: host edge guide icons, auto-hide edge zones, and container cross guide icons, with strict priority and outer-margin suppression.

## Requirements

### Requirement: Three-layer drag hit priority

During an active dock tab drag, drop target resolution SHALL follow strict priority: (1) host edge guide icon, (2) auto-hide edge zone, (3) container cross guide icon inside the inner region, (4) otherwise float or no dock.

#### Scenario: Host guide wins over auto-hide at icon

- **WHEN** the pointer is over a host edge guide icon and also inside the auto-hide edge zone on the same edge
- **THEN** the host edge dock target SHALL take priority
- **AND** auto-hide hover SHALL be cleared for that frame

#### Scenario: Auto-hide wins over cross near host edge

- **WHEN** the pointer is in the auto-hide edge zone on the bottom host edge and inside a container's outer margin
- **THEN** auto-hide edge hover SHALL be active
- **AND** container cross guides SHALL NOT be highlighted

#### Scenario: Cross only inside inner region

- **WHEN** the pointer is over a container content area outside the outer margin and not in host guide or auto-hide targets
- **THEN** container cross guide logic SHALL be eligible
- **AND** host edge dock and auto-hide SHALL be inactive

### Requirement: Outer margin suppresses container cross

The system SHALL define an outer margin inset from `m_host_rect`. Container cross hit-testing and slot resolution SHALL be disabled when the pointer lies within this margin of any host edge.

#### Scenario: Margin blocks cross near perimeter

- **WHEN** the pointer is within the outer margin of the left host edge over a Hierarchy container
- **THEN** container cross guides SHALL NOT be shown or hit-tested
- **AND** auto-hide edge zone MAY still apply if pinnable

### Requirement: Container cross hover-icon-only visibility

When container cross guides are eligible during drag, all five cross guide icons SHALL be visible at faint opacity. Only the icon under the cursor SHALL be highlighted.

#### Scenario: Faint cross on container hover

- **WHEN** the pointer is over a dock container's content area (outside outer margin) during tab drag
- **THEN** five cross guide icons SHALL appear faint at the container center layout
- **AND** none SHALL be fully highlighted until the pointer is over a specific icon

#### Scenario: Highlight single cross icon

- **WHEN** the pointer moves over the left cross guide icon of a container
- **THEN** only that icon SHALL be highlighted
- **AND** the split preview SHALL reflect the left slot for that container (~1/3 width)

### Requirement: Container cross icon-only hit target

Container split slot selection during drag SHALL be determined by which cross guide icon is under the pointer, not by nearest-edge fraction (`computeSlotRaw`) within the content rect.

#### Scenario: Icon hit selects slot

- **WHEN** the pointer is over the top cross guide icon of a container
- **THEN** the hovered slot SHALL be top
- **AND** releasing the drag SHALL split-dock into the top slot of that container

#### Scenario: Content center without icon hit

- **WHEN** the pointer is in the center of a container content area but not over any cross guide icon
- **THEN** no cross slot SHALL be hovered
- **AND** releasing the drag SHALL NOT split-dock into that container

### Requirement: Reliable in-host drag pointer updates

In-host tab drags SHALL receive continuous dock-local pointer coordinates for the full drag duration after the drag threshold is exceeded, independent of whether the pointer is over tab chrome, panel content, or host edges.

#### Scenario: Left edge mid-height updates during drag

- **WHEN** the user drags a tab along the left host edge at mid height (outside the tab strip)
- **THEN** `handleDragMove` SHALL receive updated dock-local coordinates each frame
- **AND** auto-hide or host guide resolution SHALL be able to activate along the full edge length

### Requirement: Drag controller exposes layer-specific hover state

`DockDragController` SHALL independently track host edge dock hover, auto-hide edge hover, and container cross hover such that `buildLayoutModel` can render the correct previews without mutual exclusion bugs.

#### Scenario: Host and cross previews do not conflate

- **WHEN** the pointer hovers a container cross icon while host guides are faintly visible in the background
- **THEN** only the container split preview SHALL be visible
- **AND** the half-window host preview SHALL be hidden
