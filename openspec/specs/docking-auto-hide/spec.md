# docking-auto-hide Specification

## Purpose

Qt ADS-style auto-hide docking: pin panels to edge strips, expand as overlays, resize and restore layout on unpin.

## Requirements

### Requirement: Auto-hide feature gate

The docking system SHALL support an `DockAutoHideFlag::feature_enabled` configuration flag. When disabled, all auto-hide APIs SHALL be no-ops and layout SHALL match pre-feature behavior.

#### Scenario: Feature disabled

- **WHEN** `DockManager` is constructed without `feature_enabled` and a client calls `setWidgetAutoHide`
- **THEN** the widget remains in its current dock container and no auto-hide tabs are emitted in `DockLayoutModel`

### Requirement: Pin dock widget to edge

A pinnable `DockWidget` SHALL be pin-able to one of four edges (left, right, top, bottom) via `DockManager::setWidgetAutoHide(widget_id, edge, true)`. Pinning SHALL remove the widget from the main split tree, reclaim layout space for remaining panels, and register the widget on the corresponding edge strip.

#### Scenario: Pin Hierarchy to left edge

- **WHEN** the user pins the Hierarchy panel to the left edge while the default workspace is active
- **THEN** the main split layout SHALL no longer allocate a left column for Hierarchy
- **AND** a collapsed tab labeled "Hierarchy" SHALL appear on the left auto-hide strip
- **AND** the viewport logical rect reported to `SlintSystem` SHALL increase in width accordingly

#### Scenario: Non-pinnable viewport rejected

- **WHEN** a client attempts to pin a widget with `DockPanelKind::viewport`
- **THEN** `setWidgetAutoHide` SHALL return failure without changing layout

### Requirement: Collapsed edge tab strip

Each auto-hidden widget on an edge SHALL appear as a tab in that edge's strip while collapsed. The strip SHALL consume only the configured strip thickness from the dock host and SHALL NOT show panel content.

#### Scenario: Multiple widgets on same edge

- **WHEN** Inspector and a custom Console panel are both pinned to the right edge
- **THEN** both tabs SHALL be visible in the right strip in stable order
- **AND** neither panel's content SHALL be visible until expanded

### Requirement: Expand and collapse overlay

Clicking a collapsed auto-hide tab SHALL expand that widget in an overlay above the main dock content. The overlay SHALL include the panel content region and an inner-edge resize handle. Clicking the same tab again (or invoking collapse) SHALL hide the overlay while keeping the tab on the strip.

#### Scenario: Click tab to expand

- **WHEN** the user clicks a collapsed "Inspector" tab on the right strip
- **THEN** an overlay containing the Inspector panel SHALL appear above the dock host content
- **AND** the overlay SHALL include a resize handle on the edge adjacent to the workspace (left edge for right-side panels)

#### Scenario: Click tab to collapse

- **WHEN** the Inspector overlay is expanded and the user clicks the Inspector strip tab again
- **THEN** the overlay SHALL hide
- **AND** the Inspector tab SHALL remain on the right strip

### Requirement: Unpin restores docked layout

`DockManager::unpinAutoHide(widget_id)` SHALL remove the widget from the auto-hide strip and restore it to the main dock tree using a stored restore hint when valid, or dock to a sensible default edge slot when the hint is invalid.

#### Scenario: Unpin after pin

- **WHEN** the user unpins Hierarchy from the left auto-hide strip
- **THEN** Hierarchy SHALL reappear as a docked panel in the main split tree
- **AND** the left strip SHALL no longer show a Hierarchy tab

### Requirement: Overlay resize handle

An expanded auto-hide overlay SHALL expose a resize handle on its inner edge. Dragging the handle SHALL adjust the overlay size between a minimum of 64 logical pixels and a maximum derived from the dock host size minus strip insets.

#### Scenario: Resize right-side overlay

- **WHEN** the user drags the left resize handle of an expanded right-edge Inspector overlay wider
- **THEN** the overlay content width SHALL increase until the maximum allowed size is reached

### Requirement: Hover to expand and collapse

When `DockAutoHideFlag::show_on_mouse_over` is set, hovering a collapsed tab SHALL schedule expand after a short delay, and leaving the tab or overlay SHALL schedule collapse after a short delay. Mouse button press on the tab SHALL cancel pending hover timers.

#### Scenario: Hover expands collapsed tab

- **WHEN** `show_on_mouse_over` is enabled and the pointer enters a collapsed tab for longer than the configured delay
- **THEN** the corresponding overlay SHALL expand without a click

#### Scenario: Leave collapses expanded overlay

- **WHEN** `show_on_mouse_over` is enabled, an overlay is expanded via hover, and the pointer leaves both the tab and overlay for longer than the configured delay
- **THEN** the overlay SHALL collapse to strip-only state

### Requirement: Click outside to collapse

When `DockAutoHideFlag::close_on_outside_click` is set, a mouse release outside any expanded auto-hide overlay SHALL collapse all expanded auto-hide overlays without unpinning.

#### Scenario: Outside click collapses

- **WHEN** Inspector is expanded as an auto-hide overlay and the user clicks on the viewport area outside the overlay frame
- **THEN** the Inspector overlay SHALL collapse
- **AND** the Inspector tab SHALL remain pinned on the strip

### Requirement: Close button collapse semantics

When `DockAutoHideFlag::close_button_collapses` is set, the close affordance on an auto-hidden widget SHALL collapse the overlay instead of removing the widget from the session. When the flag is clear, close SHALL behave as the standard dock close (detach widget).

#### Scenario: Close collapses instead of destroying

- **WHEN** `close_button_collapses` is enabled and the user clicks close on an expanded auto-hidden Inspector
- **THEN** the overlay SHALL collapse
- **AND** the Inspector tab SHALL remain on the auto-hide strip

### Requirement: Layout model exposes auto-hide geometry

`DockLayoutModel` SHALL include auto-hide tab and overlay views sufficient for Slint to render strips and expanded frames without querying `DockManager` internals.

#### Scenario: Sync to Slint

- **WHEN** `SlintSystem::syncDockingWorkspace` runs after a pin operation
- **THEN** the editor window SHALL receive updated `docking-auto-hide-tabs` and `docking-auto-hide-overlays` models reflecting strip and overlay rectangles

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
