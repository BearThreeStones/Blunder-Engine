## ADDED Requirements

### Requirement: Stable dock slot selection during tab drag

`DockManager::computeSlot` SHALL select a dock slot (left, right, top, bottom, center) without flickering when the pointer moves sub-pixel distances near region boundaries. Slot changes SHALL require crossing a hysteresis band or exceeding a minimum dwell threshold at the new region.

#### Scenario: Pointer near 25% edge boundary does not oscillate

- **WHEN** the user holds the pointer steady near the boundary between center and an edge slot
- **THEN** the active slot and highlighted guide SHALL remain stable
- **AND** the slot SHALL NOT alternate between center and an edge each frame

#### Scenario: Corner region uses deterministic tie-break

- **WHEN** the pointer is in a corner region where multiple edge distances are equal within epsilon
- **THEN** the system SHALL select the nearest edge by normalized distance without relying on floating-point equality (`==`)
- **AND** the highlighted guide SHALL match the computed slot

#### Scenario: Main window tab merge shows stable center guide

- **WHEN** the user drags a tab over the center of a target container tab bar in the main window
- **THEN** the center guide SHALL highlight consistently
- **AND** the preview rect SHALL remain centered until the pointer leaves the center region

### Requirement: Guide highlight matches computed slot

The dock layout model guide with `highlighted == true` SHALL always correspond to the slot returned by `computeSlot` for the current hover target. Preview rect from `previewRectForSlot` SHALL use the same slot.

#### Scenario: Left guide highlight matches drop

- **WHEN** the left guide is highlighted during tab drag
- **THEN** releasing over that guide SHALL dock into `DockSlot::left` on the hovered container
- **AND** the preview rect SHALL occupy the left half of the target container
