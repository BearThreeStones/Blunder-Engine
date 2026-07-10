## ADDED Requirements

### Requirement: Outer-edge auto-hide drop detection during drag

While a dock tab drag is active, the system SHALL detect when the pointer lies within the host outer auto-hide zone on a dock edge and SHALL treat that as an auto-hide drop target when the dragged widget is pinnable and auto-hide is enabled. The outer zone depth SHALL default to `auto_hide_drop_preview_size` (32 logical pixels) and SHALL span the full edge length (full host height for left/right, full host width for top/bottom).

Pointer positions for in-host tab drags SHALL be updated via global pointer poll after the drag threshold is exceeded, so detection continues when the cursor leaves the tab `TouchArea`.

#### Scenario: Pointer in outer left band

- **WHEN** the user drags a pinnable widget and the pointer is within the outer auto-hide zone of the left edge of `m_host_rect` (32px deep, any y along the edge)
- **THEN** the drag controller SHALL set the hovered auto-hide edge to left
- **AND** SHALL NOT treat the hover as a split-dock left slot unless the pointer leaves the outer band

#### Scenario: Guide hit along full edge length

- **WHEN** the user drags toward the left perimeter guide icon but the pointer is above or below the centered icon (still within the guide hit band depth from the host left edge)
- **THEN** the system SHALL still activate the left auto-hide drop target
- **AND** SHALL NOT require the pointer to be inside the small visual icon rect only

#### Scenario: In-host drag along full edge after leaving tab strip

- **WHEN** the user drags a pinnable tab from a center or bottom panel toward the host left edge and the pointer is no longer over the tab `TouchArea`
- **THEN** `handleDragMove` SHALL still receive dock-local pointer updates each frame
- **AND** the left auto-hide drop target SHALL activate anywhere along the left edge within the guide or outer hit band depth

#### Scenario: Non-pinnable widget ignores outer band

- **WHEN** the user drags a non-pinnable widget (e.g. viewport) near the host outer edge
- **THEN** the system SHALL NOT activate an auto-hide drop target
- **AND** split-dock guides MAY still appear when hovering a container interior

#### Scenario: Mouse zone widens when strip visible

- **WHEN** the left auto-hide strip already has tabs and the pointer is within the strip thickness from the host left edge
- **THEN** the auto-hide mouse zone for the left edge SHALL be at least the strip thickness
- **AND** when no strip exists the zone depth SHALL be `auto_hide_drop_preview_size` (32px), not 8px

### Requirement: Auto-hide perimeter guides with strip preview

During an active tab drag of a pinnable widget, the layout model SHALL expose four perimeter auto-hide guide buttons on the host edges (top, bottom, left, right) for the **entire drag duration**. Each guide SHALL use `DockAutoHideGuideButton` with edge-appropriate icon orientation: **landscape** mini-window (48×28) on top/bottom, **portrait** mini-window (28×48) on left/right, each with an embedded white inward arrow. When an auto-hide edge is hovered, the UI SHALL also show the narrow strip preview on that edge.

#### Scenario: Perimeter guides visible throughout pinnable drag

- **WHEN** the user drags a pinnable Hierarchy tab anywhere over the dock host
- **THEN** four auto-hide guide buttons SHALL be visible on the host top, bottom, left, and right edges
- **AND** top/bottom guides SHALL use landscape mini-window icons with a white inward-pointing arrow on the host-outer side
- **AND** left/right guides SHALL use **portrait** (taller than wide) mini-window icons with a white inward-pointing arrow on the host-outer side
- **AND** the guides SHALL remain visible even when the pointer is over a container interior showing split cross guides

#### Scenario: Left and right portrait orientation

- **WHEN** the left or right perimeter auto-hide guide is visible during a pinnable drag
- **THEN** the mini-window icon SHALL be **portrait** (height greater than width, 28×48 logical px)
- **AND** the icon SHALL NOT use the same landscape aspect ratio as top/bottom guides

#### Scenario: Perimeter guide distinct from split cross

- **WHEN** perimeter auto-hide guides and split cross guides are visible simultaneously
- **THEN** perimeter guides SHALL NOT use half-pane fills or dashed dividers
- **AND** split cross guides SHALL continue to use half-pane fills and dashed dividers per §7.8

#### Scenario: Perimeter guide highlight

- **WHEN** the user hovers an auto-hide perimeter guide
- **THEN** the guide SHALL show a bolder blue frame (`#6ab0ff`, 2px)
- **AND** the guide body fill and arrow color SHALL remain unchanged from the inactive state

#### Scenario: Narrow strip preview on hovered edge

- **WHEN** the user hovers the left auto-hide guide or outer left drop zone while dragging a pinnable panel
- **THEN** the UI SHALL show a semi-transparent preview strip on the left edge
- **AND** the left perimeter guide SHALL be highlighted
- **AND** the preview width SHALL default to 32 logical pixels when no strip exists, or the existing strip thickness when tabs are already present

#### Scenario: Non-pinnable drag hides perimeter guides

- **WHEN** the user drags a non-pinnable widget (e.g. viewport)
- **THEN** auto-hide perimeter guides SHALL NOT be shown
- **AND** auto-hide strip preview SHALL NOT appear on release at outer edges

### Requirement: ADS-style dark-theme split cross guides

Split-dock cross guides SHALL use ADS-style **mini-window** pictographic buttons adapted to the editor dark theme: blue outer frame, solid top chrome bar, semi-transparent active half-pane, inactive dark half-pane, and a **dashed** divider at the split boundary (not a solid line or single-edge bar).

#### Scenario: Cross guide mini-window appearance

- **WHEN** the user hovers a dock container interior during tab drag
- **THEN** five split guides SHALL appear in a cross layout at the container center
- **AND** each guide SHALL render as a mini-window icon with a blue frame, top chrome bar, and slot-appropriate active/inactive halves
- **AND** edge slots (left, right, top, bottom) SHALL show a dashed divider between active and inactive halves
- **AND** the highlighted guide SHALL use a brighter frame and stronger active fill than inactive guides

#### Scenario: Center guide tab-merge glyph

- **WHEN** the center split guide is visible during tab drag
- **THEN** the center guide SHALL show full-content active fill (tab merge) without a dashed divider

#### Scenario: Split and auto-hide guides coexist

- **WHEN** the user drags a pinnable tab over a container interior
- **THEN** split cross guides SHALL be visible on the container
- **AND** host perimeter auto-hide guides SHALL remain visible simultaneously

### Requirement: Drop on auto-hide edge pins widget

Releasing the drag pointer while an auto-hide edge is hovered SHALL pin the dragged widget to that edge using the existing auto-hide entry lifecycle (`detachWidget`, strip tab, collapsed state).

#### Scenario: Drag tab to left edge and release

- **WHEN** the user drags a docked Hierarchy tab to the left auto-hide drop zone and releases
- **THEN** Hierarchy SHALL be removed from the main split tree
- **AND** a collapsed Hierarchy tab SHALL appear on the left auto-hide strip
- **AND** the main layout SHALL reclaim space previously used by Hierarchy

#### Scenario: Drop from native OS float

- **WHEN** the user drags a tab from a native OS floating panel to the left auto-hide drop zone and releases
- **THEN** the dragged widget SHALL be pinned to the left edge
- **AND** the native float window for that drag source SHALL be torn down or reconciled without leaving a blank float

### Requirement: Auto-hide guide hit targets

Auto-hide drop detection SHALL treat perimeter guide button rectangles as valid hit targets in addition to the outer mouse zone band.

#### Scenario: Click-release on visible left guide

- **WHEN** the user releases a pinnable tab drag with the pointer over the left perimeter guide button
- **THEN** the widget SHALL pin to the left auto-hide edge
- **AND** the drop SHALL succeed when the pointer is anywhere in the guide hit band or outer preview-width zone for that edge

### Requirement: Auto-hide tab insert index on drop

When dropping onto an edge that already has auto-hide tabs, the system SHALL insert the new tab at an index derived from the pointer position along that strip, matching ADS ordering semantics.

#### Scenario: Insert before existing tab

- **WHEN** the user drops a pinnable widget on the right auto-hide edge with the pointer above the first existing tab on that edge
- **THEN** the new tab SHALL appear before the first tab on the right strip

#### Scenario: Insert after last tab

- **WHEN** the user drops with the pointer below the last tab on the bottom strip
- **THEN** the new tab SHALL appear after the last tab on the bottom strip

### Requirement: Split drop preview uses wider band than auto-hide

Split-dock drop previews SHALL use a substantially wider preview rectangle than auto-hide strip previews so users can distinguish the two modes during drag.

#### Scenario: Left split preview width

- **WHEN** the user hovers the inner left split zone of a container 300px wide
- **THEN** the split preview SHALL use approximately one-third of the container width (~100px)
- **AND** the preview SHALL be visibly wider than the auto-hide strip preview on the host outer edge

## MODIFIED Requirements

### Requirement: Auto-hide drag preview without cross icons

**Reason:** Superseded by ADS-style perimeter guide buttons plus strip preview. Auto-hide mode now shows dedicated edge guide icons (not split cross icons) alongside strip preview on hover.

**Migration:** Replace flat strip-only overlay with `DockAutoHideGuidesLayer` + `DockAutoHideGuideButton`; remove `editor_window.slint` mutual exclusion between split guides and auto-hide preview.

#### Scenario: Auto-hide hover does not use split cross icons

- **WHEN** the user hovers an auto-hide perimeter guide or outer edge drop zone
- **THEN** the auto-hide strip preview SHALL be shown on the hovered edge
- **AND** the five-direction split cross guides on a container SHALL NOT be highlighted for that auto-hide hover state
- **AND** perimeter auto-hide guide buttons (not split cross buttons) SHALL indicate the auto-hide target
