# dock-tab-click-activation Specification

## Purpose

Tab click vs tab drag semantics for multi-widget dock containers: clicking activates without mutating the dock tree; edge-slot detection excludes the chrome row.

## Requirements

### Requirement: Tab click activates without dock mutation

In a dock container with more than one widget, pressing and releasing a tab without moving the pointer beyond the configured tab-drag threshold SHALL activate that widget and SHALL NOT change the dock tree (no split, float, auto-hide pin, or reparent).

#### Scenario: Click inactive tab in multi-tab container

- **WHEN** the user presses and releases a tab in a container that has two or more widgets
- **AND** pointer movement from press to release is below the tab-drag threshold
- **THEN** the pressed widget SHALL become the active widget in that container
- **AND** the dock tree SHALL remain a single container with the same widgets (no new split nodes)

#### Scenario: Click does not create vertical split

- **WHEN** the user center-merged a second panel into an existing tab container
- **AND** the user clicks the other tab without dragging past the threshold
- **THEN** the layout SHALL show one content area with tab switching
- **AND** SHALL NOT show a top/bottom split of the two panels

### Requirement: Tab bar excluded from split edge detection during drag

While a tab drag is active, edge slot detection (left, right, top, bottom) on a hovered container SHALL use the container content rectangle (below the integrated chrome row), not the full rectangle including the tab bar.

#### Scenario: Pointer over tab bar during drag does not select top edge

- **WHEN** a tab drag is active and the pointer is over the chrome/tab bar band of a container
- **THEN** the system SHALL NOT select `DockSlot::top` (or any edge slot) solely because of tab-bar geometry
- **AND** SHALL NOT highlight edge split guides for chrome-only hover

#### Scenario: Content-area edge drag still splits

- **WHEN** a tab drag exceeds the threshold and the pointer is over the bottom third of a container's **content** area
- **THEN** the system SHALL offer bottom-edge split guide and preview as today
