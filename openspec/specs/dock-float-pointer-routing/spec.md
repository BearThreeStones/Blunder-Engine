# dock-float-pointer-routing Specification

## Purpose
TBD - created by archiving change dock-float-redock-guide-stability. Update Purpose after archive.
## Requirements
### Requirement: Scoped global pointer poll for native float drags

When a dock tab drag is active, `SlintSystem::tickGlobalDockPointerPoll` SHALL call `DockManager::handleDragMove` only if the drag originated from a widget inside a native OS floating window. In-host tab drags SHALL be driven exclusively by Slint `tab-moved` callbacks without per-frame poll overwrite.

#### Scenario: In-host tab drag uses Slint coordinates only

- **WHEN** the user drags a tab from a panel hosted in the main editor window
- **THEN** `handleDragMove` SHALL receive coordinates from Slint `tab-moved` events
- **AND** `tickGlobalDockPointerPoll` SHALL NOT call `handleDragMove` for that drag

#### Scenario: Native float tab drag uses global poll

- **WHEN** the user drags a tab from a native OS floating window
- **THEN** `tickGlobalDockPointerPoll` SHALL call `handleDragMove` each frame while the left button is held
- **AND** `endDrag` SHALL be invoked when the left button is released

#### Scenario: Native float resize still uses global poll

- **WHEN** the user resizes a native OS floating window by edge or grip
- **THEN** `tickGlobalDockPointerPoll` SHALL continue to drive `updateFloatingInteraction` while the left button is held

### Requirement: Unified pointer to dock-local conversion

Pointer positions for native-float tab drags SHALL be converted to main-window dock-local coordinates using a single helper that selects the correct path based on cursor location: `floatLocalToDockLocal` when the cursor is over the source or any owned child float HWND, and `screenToDockLocal` when over the main editor client area. Both paths SHALL use the same HiDPI scale factors as `mapWindowPointerToLogical` / `mapSdlWindowPointerToLogical`.

#### Scenario: Cursor over native float during tab drag

- **WHEN** the global cursor is inside a native floating child window client rect during tab drag
- **THEN** dock-local coordinates SHALL be computed via child-window client position plus `docking_origin` offset
- **AND** the result SHALL match in-host tab-drag coordinates at the same screen pixel

#### Scenario: Cursor over main dock host during tab drag from float

- **WHEN** the global cursor is inside the main editor client area during tab drag from a native float
- **THEN** dock-local coordinates SHALL be computed via `ScreenToClient` on the main HWND minus `docking_origin`
- **AND** `hitTest` SHALL return a valid container when the cursor is over docked panel chrome

### Requirement: Hide native float during tab drag

When `beginDrag` is invoked from a tab inside a native OS floating window, the system SHALL hide that float's SDL child window until `endDrag` or `cancelDrag` completes. On cancel or failed drop, the window SHALL be shown again at its prior position.

#### Scenario: Float hidden while dragging tab back

- **WHEN** the user begins dragging a tab from a native floating window
- **THEN** the source native OS window SHALL be hidden
- **AND** dock guides and drag preview on the main window SHALL remain visible when the cursor is over the main dock host

#### Scenario: Float restored on cancel

- **WHEN** the user cancels a tab drag from a native float without a valid guide drop
- **THEN** the native OS window SHALL be shown again
- **AND** the tab SHALL remain in the floating container

