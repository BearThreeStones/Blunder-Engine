## Why

After merging two panels into one tab container via the center dock guide, clicking an inactive tab to switch panels incorrectly commits a **vertical split** (top/bottom panes) instead of only changing the active tab. Tab `pointer down` activates the widget and immediately starts a drag; on release, `handleDragMove` treats the pointer over the tab bar as a **top-edge** drop on the same container and `endDrag` calls `insertSplit`. This breaks basic multi-tab UX and is unrelated to the auto-hide drag-drop work.

## What Changes

- Distinguish **tab click** (pointer down/up within drag threshold) from **tab drag** (movement past threshold) so release without meaningful movement does not dock, float, or split.
- Exclude the chrome/tab bar band from edge-slot hit testing during drag hover so tab-bar coordinates are not interpreted as top-edge split targets.
- Preserve intentional tab-drag behaviors: drag preview after threshold, re-dock via guides, OS float on release outside guides, and edge splits when the pointer is over **content** areas.

## Capabilities

### New Capabilities

- `dock-tab-click-activation`: Clicking a tab in a multi-widget container activates that widget without mutating the dock tree (no split, float, or auto-hide on a no-move release).

### Modified Capabilities

- `dock-browser-tab-chrome`: Clarify that tab press starts drag lifecycle only after crossing the configured drag threshold; a click without threshold movement SHALL activate only.

## Impact

- `engine/src/runtime/function/ui/docking/dock_manager.cpp` — `endDrag`, `handleDragMove` / `computeSlotRaw` (content rect), possibly `beginDrag` coordination
- `engine/src/runtime/function/ui/docking/dock_drag_controller.cpp` — expose or reuse drag-distance / threshold helpers
- `engine/src/runtime/function/ui/docking/dock_slint_bridge.cpp` — optional ordering if Slint defers `tab-pressed` until threshold (design decision)
- `engine/src/runtime/function/slint/docking_panel.slint` — only if Slint-side threshold deferral is chosen
- Tests: extend docking unit tests for click-vs-drag endDrag and tab-bar slot exclusion
