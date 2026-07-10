# dock-floating-os-window Specification

## Purpose
TBD - created by archiving change dock-float-redock-guide-stability. Update Purpose after archive.
## Requirements
### Requirement: Re-dock from native float

Dragging a tab from a native floating window back onto the main dock host SHALL show dock guides and drag preview on the main window, and on valid guide drop SHALL dock the widget into the target container and destroy the native window if the float becomes empty. Guide visibility SHALL NOT depend on the native float window remaining visible during the drag.

#### Scenario: Re-dock Inspector from OS window

- **WHEN** the user drags the Inspector tab from a native float onto a valid center guide on the main dock
- **THEN** Inspector SHALL dock into the target area
- **AND** the native floating window SHALL close when it has no remaining widgets

#### Scenario: Guides visible with float moved aside

- **WHEN** the user moves the native floating window away from the main dock and drags its tab over the main dock host
- **THEN** dock guides and preview SHALL appear on the main window
- **AND** a valid guide drop SHALL complete re-dock

#### Scenario: Edge guide drop from native float

- **WHEN** the user drags a tab from a native float onto a highlighted left guide on the main dock
- **THEN** the widget SHALL dock into the left slot of the target container
- **AND** the native window SHALL be destroyed if the float is empty

### Requirement: Screen-space coordinate sync

`DockManager` floating frame rects SHALL be stored in main-window client coordinates for layout solving, while native windows SHALL be positioned using screen-space conversion from the main HWND client origin. Tab-drag pointer positions used for `hitTest` and `computeSlot` on the main dock host SHALL use the same dock-local coordinate space as in-host tab drags, including under per-monitor HiDPI scaling.

#### Scenario: Float position matches visual placement

- **WHEN** a native float is created at the pointer release position
- **THEN** the window SHALL appear under the cursor with the same offset semantics as the prior in-host float (centered on pointer with title-bar bias)

#### Scenario: Re-dock hit test aligns with visual guides

- **WHEN** the cursor is over a visible dock guide on the main window during tab drag from a native float
- **THEN** `hitTest` and `computeSlot` SHALL agree with the guide positions shown in the layout model

