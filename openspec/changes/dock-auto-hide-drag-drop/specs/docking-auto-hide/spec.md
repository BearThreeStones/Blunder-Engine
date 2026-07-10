## ADDED Requirements

### Requirement: Drag-to-pin complements pin button

The auto-hide system SHALL support pinning a pinnable dock widget to an edge strip by dragging its tab to the host outer auto-hide drop zone or perimeter guide button, in addition to the existing pin-button path (`pinWidgetToDefaultEdge` / `setWidgetAutoHide`).

#### Scenario: Pin button still works after drag-to-pin ships

- **WHEN** the user clicks the pin affordance on a docked Inspector tab without dragging
- **THEN** Inspector SHALL pin to its default edge per existing panel-kind rules
- **AND** behavior SHALL match pre-drag-to-pin releases

#### Scenario: Unpin unchanged

- **WHEN** the user unpins a widget from the auto-hide strip via the strip unpin control
- **THEN** the widget SHALL restore to the main dock tree per existing restore-hint rules

### Requirement: Perimeter guide arrows during drag-to-pin

When drag-to-pin is active for a pinnable widget, each host-edge auto-hide guide SHALL display a white inward-pointing triangle arrow **inside** the Slint guide component (on the host-outer side of the flat-wide mini-window icon), indicating pin direction toward the dock interior.

#### Scenario: Left edge arrow

- **WHEN** the user drags a pinnable tab and the left perimeter guide is visible
- **THEN** a white triangle arrow SHALL appear on the outer (left) side of the guide icon, pointing right (inward)
- **AND** the arrow SHALL be part of the same guide component bounds used for hit-testing (no separate C++ arrow rectangle)

#### Scenario: Arrows hidden for non-pinnable drag

- **WHEN** the user drags a non-pinnable widget
- **THEN** perimeter guide arrows SHALL NOT be shown

### Requirement: Auto-hide feature gate applies to drag-to-pin

When `DockAutoHideFlag::feature_enabled` is disabled, drag-to-pin drop zones and previews SHALL be disabled alongside existing auto-hide APIs.

#### Scenario: Feature disabled

- **WHEN** auto-hide feature is disabled and the user drags a pinnable tab to the host outer edge
- **THEN** no auto-hide strip preview SHALL appear
- **AND** release SHALL fall back to existing split-dock or float behavior only
