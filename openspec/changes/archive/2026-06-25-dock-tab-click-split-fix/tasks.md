## 1. Drag controller threshold API

- [x] 1.1 Add `DockDragController::exceededDragThreshold()` (distance from `m_drag_start_pointer` to `m_pointer` >= `m_threshold_px`)
- [x] 1.2 Pass `DockDragController::k_default_drag_threshold` from `DockManager::beginDrag` instead of `tab_bar_height` so click/drag gating matches preview spec (8px)

## 2. endDrag click guard

- [x] 2.1 In `DockManager::endDrag`, if drag active and `!exceededDragThreshold()`, reset stabilizer and `m_drag.reset()` without `dockWidget`, `makeFloatingFor`, or auto-hide drop
- [x] 2.2 Ensure cancelled drags and native-float edge cases still behave as before when threshold is exceeded

## 3. Content-rect slot detection

- [x] 3.1 In `handleDragMove`, when target is a container, derive content rect (container rect with top inset `chrome_bar_height`) for `computeSlotRaw` / `stabilizeSlot` / `previewRectForSlot`
- [x] 3.2 When pointer is only over the tab bar band (outside content rect), clear hover or force center — no edge guide highlight on chrome-only position

## 4. Verification

- [x] 4.1 Build `engine_editor` / `engine_runtime` (Debug)
- [x] 4.2 Manual: center-merge two panels → click other tab → content switches, no vertical split
- [x] 4.3 Manual: drag tab past threshold to top/bottom content guide → split still works
- [x] 4.4 Manual: click tab without move → no OS float, no auto-hide pin
- [x] 4.5 Manual: drag tab from multi-tab container to another container center → merge still works
