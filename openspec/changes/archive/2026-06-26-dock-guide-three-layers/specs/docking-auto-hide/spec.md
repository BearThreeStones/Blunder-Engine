## ADDED Requirements

### Requirement: Drag-to-pin via edge mouse zone only

During an active tab drag of a pinnable widget, auto-hide pin targeting SHALL be determined solely by the pointer lying within the configured thin edge mouse zone on a host edge. Perimeter guide icon hit rectangles SHALL NOT be used for auto-hide drag detection.

#### Scenario: Edge zone on left triggers auto-hide

- **WHEN** the user drags a pinnable Hierarchy tab to within the left auto-hide edge zone along mid height (not over a host guide icon)
- **THEN** the drag controller SHALL set the hovered auto-hide edge to left
- **AND** a narrow strip preview SHALL appear on the left edge

#### Scenario: All four edges support drag-to-pin

- **WHEN** the user drags a pinnable widget into the edge zone on the top, bottom, left, or right host edge
- **THEN** the corresponding auto-hide edge SHALL be eligible for drop on release
- **AND** pinning SHALL use the existing `setWidgetAutoHide` lifecycle

#### Scenario: Guide icon hit does not trigger auto-hide

- **WHEN** the pointer is over a host edge guide icon but outside the auto-hide edge zone depth
- **THEN** auto-hide hover SHALL NOT be activated
- **AND** host edge dock hover MAY be activated instead

### Requirement: Auto-hide drag decoupled from perimeter guide icons

The drag overlay SHALL NOT require or display dedicated auto-hide perimeter guide buttons (`DockAutoHideGuideButton`) for drop targeting. Strip preview on the hovered edge is sufficient visual feedback for auto-hide drag.

#### Scenario: No auto-hide perimeter icons during drag

- **WHEN** the user drags a pinnable widget over the dock host
- **THEN** the system SHALL NOT show `DockAutoHideGuidesLayer` perimeter icons for drag-to-pin
- **AND** auto-hide targeting SHALL rely on edge zone geometry only

#### Scenario: Strip preview on edge hover

- **WHEN** the pointer enters the bottom auto-hide edge zone during a pinnable drag
- **THEN** a narrow strip preview SHALL appear along the bottom host edge
- **AND** no separate auto-hide guide icon highlight is required

## MODIFIED Requirements

### Requirement: Dock interaction defer hooks

`isSplitterResizeInteractionActive` SHALL return true during dock splitter drags and auto-hide overlay resize. `isDockLayoutDragActive` SHALL return true during dock tab drag for input routing. Dock tab drag SHALL NOT block Skia UI composite or animation timers (host guides, auto-hide strip previews, and cross guides must update live during drag).

#### Scenario: Auto-hide resize defers heavy work

- **WHEN** the user is dragging an auto-hide overlay resize handle
- **THEN** `SlintSystem::isSplitterResizeInteractionActive` SHALL return true
- **AND** Skia composite MAY be throttled without starving layout feedback

#### Scenario: Dock tab drag keeps UI live

- **WHEN** the user is dragging a dock tab
- **THEN** `SlintSystem::isDockLayoutDragActive` SHALL return true
- **AND** `shouldDeferHeavyFrameWork` SHALL remain false so host edge guides, auto-hide strip previews, and cross guides refresh during the drag
