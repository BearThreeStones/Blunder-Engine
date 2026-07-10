## ADDED Requirements

### Requirement: Native floating window on commit

When a dock tab drag ends without a valid dock-guide drop and the panel is eligible for native float, the system SHALL create a native OS child window for that floating container and render the panel chrome and content in that window instead of the in-host `docking-floatings` rectangle.

#### Scenario: Hierarchy tab floated to OS window

- **WHEN** the user drags the Hierarchy tab outside valid dock guides and releases over the desktop area
- **THEN** a native child window SHALL appear at the release position
- **AND** the Hierarchy panel content SHALL be visible inside that window
- **AND** the in-host `docking-floatings` entry for that float SHALL NOT be shown

#### Scenario: Viewport remains in main window

- **WHEN** the user attempts to float the Viewport panel
- **THEN** the system SHALL NOT create a native OS window for Viewport
- **AND** Viewport SHALL remain hosted in the main editor Slint tree with the existing Vulkan viewport Image path

### Requirement: Floating window lifecycle

Each native floating window SHALL support move (title-bar drag), resize (grip), close, and z-order raise on activation. Destroying the window or closing it SHALL remove the floating node from `DockManager` or re-dock per existing close semantics.

#### Scenario: Move native float by title bar

- **WHEN** the user drags the title bar of a native floating window
- **THEN** the OS window position SHALL update to follow the pointer
- **AND** the dock layout model SHALL reflect the new frame rect

#### Scenario: Close native float

- **WHEN** the user clicks close on a native floating window title bar
- **THEN** the floating container SHALL be torn down
- **AND** the native window SHALL be destroyed

### Requirement: Re-dock from native float

Dragging a tab from a native floating window back onto the main dock host SHALL show dock guides and, on valid drop, SHALL dock the widget into the target container and destroy the native window if the float becomes empty.

#### Scenario: Re-dock Inspector from OS window

- **WHEN** the user drags the Inspector tab from a native float onto a valid center guide on the main dock
- **THEN** Inspector SHALL dock into the target area
- **AND** the native floating window SHALL close when it has no remaining widgets

### Requirement: Windows-first with fallback flag

Native OS floating windows SHALL be enabled on Windows via a `DockFloatingFlag::native_os_window` (or equivalent) feature gate. When disabled or on unsupported platforms, the system SHALL fall back to the existing in-host `docking-floatings` implementation without changing dock tree semantics.

#### Scenario: Feature disabled

- **WHEN** `native_os_window` is false
- **THEN** committed floats SHALL use in-host `docking-floatings` chrome as today
- **AND** no additional SDL child windows SHALL be created

### Requirement: Screen-space coordinate sync

`DockManager` floating frame rects SHALL be stored in main-window client coordinates for layout solving, while native windows SHALL be positioned using screen-space conversion from the main HWND client origin.

#### Scenario: Float position matches visual placement

- **WHEN** a native float is created at the pointer release position
- **THEN** the window SHALL appear under the cursor with the same offset semantics as the prior in-host float (centered on pointer with title-bar bias)
