## ADDED Requirements

### Requirement: Non-Opaque drag preview on tab move

While a dock tab is dragged, after the pointer moves beyond a configured threshold, the editor SHALL show a semi-transparent floating preview (frame + title) that follows the pointer without committing a dock tree change until mouse release.

#### Scenario: Preview appears after threshold

- **WHEN** the user presses a dock tab and moves the pointer beyond the drag threshold
- **THEN** a semi-transparent preview rectangle SHALL appear at the pointer
- **AND** the preview SHALL display the dragged panel title
- **AND** no native OS window and no committed `DockNode` float SHALL be created yet

#### Scenario: Preview follows pointer

- **WHEN** the drag preview is active and the user moves the mouse
- **THEN** the preview position SHALL update each frame to follow the pointer
- **AND** dock guide overlays on the main host SHALL continue to update from the same pointer position

### Requirement: Preview visual style

The drag preview SHALL render at approximately 60% opacity with a highlighted border (ADS Non-Opaque style). Content pixmap inside the preview is optional in v1; title text is required.

#### Scenario: Preview is visibly semi-transparent

- **WHEN** the drag preview is shown over the main dock UI
- **THEN** underlying panels SHALL remain partially visible through the preview fill
- **AND** the preview SHALL NOT be fully opaque

### Requirement: Release commits or cancels preview

On mouse release, the drag preview SHALL be hidden. If a valid dock guide is hovered, the widget SHALL dock; otherwise, if the release is eligible for float, the system SHALL commit float (native OS window when enabled, else in-host). If the drag is cancelled (e.g. Escape), the preview SHALL disappear with no layout change.

#### Scenario: Release on valid guide docks

- **WHEN** the user releases the tab while a dock guide is highlighted
- **THEN** the preview SHALL hide immediately
- **AND** `dockWidget` SHALL run for the hovered slot
- **AND** no floating window SHALL be created

#### Scenario: Release outside guide creates float

- **WHEN** the user releases the tab with no valid guide and the pointer is eligible for float
- **THEN** the preview SHALL hide
- **AND** a committed float SHALL be created (native or in-host per feature flag)

#### Scenario: Escape cancels preview

- **WHEN** the user presses Escape during an active drag preview
- **THEN** the preview SHALL hide
- **AND** the tab SHALL remain in its original container

### Requirement: Preview does not block dock guides

The drag preview layer SHALL be drawn above dock tiles but SHALL NOT intercept hit-testing for dock guides on the main host; guide hit-test SHALL use the raw pointer position in dock-local coordinates.

#### Scenario: Guides remain usable under preview

- **WHEN** the drag preview overlaps a dock guide region
- **THEN** the guide under the cursor SHALL still highlight
- **AND** drop onto that guide SHALL succeed on release
