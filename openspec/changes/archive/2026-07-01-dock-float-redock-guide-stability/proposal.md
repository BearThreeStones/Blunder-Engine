## Why

Phase B native OS floating windows (`dock-floating-os-window-drag-preview`) landed with manual validation failures: tabs dragged from OS floats cannot re-dock (no guides appear even when the float is moved aside), main-window tab drags show erratic dock-guide direction (jumping between edges and center), and native float edge resize works but the OS cursor never updates. These regressions block completing the floating-window milestone and undermine trust in docking UX.

## What Changes

- Restrict `tickGlobalDockPointerPoll` to drags that **originate from native OS floats** (and float resize), so in-host tab drags are driven only by Slint `tab-moved` coordinates.
- Unify pointer→dock-local conversion for native-float tab drags: use `floatLocalToDockLocal` while the cursor is over a child float HWND, `screenToDockLocal` over the main client area, aligned with existing HiDPI mapping.
- Hide the source native OS float window during tab drag from that float; restore on cancel or after drop.
- Stabilize `DockManager::computeSlot` with hysteresis and improved corner tie-breaking so guide highlight and preview rect do not flicker at region boundaries.
- Propagate resize cursors on native float child windows via explicit OS cursor handling (`SDL_SetCursor` / Win32 `WM_SETCURSOR`); optional Slint edge hover tint for visual feedback.
- Add validation tasks covering re-dock from OS float, stable main-window guides, and edge-resize cursor.

## Capabilities

### New Capabilities

- `dock-float-pointer-routing`: Scoped global pointer poll, unified dock-local coordinate conversion, and hide-float-during-tab-drag behavior for native OS floats.
- `dock-guide-slot-stability`: Stable dock-slot selection and guide highlighting during tab drag (hysteresis, corner tie-break).
- `dock-float-resize-cursor`: OS-level resize cursor feedback on native floating window edges and grip.

### Modified Capabilities

- `dock-floating-os-window` (change delta): Re-dock from native float SHALL show guides and complete drop when hovering a valid guide; coordinate sync SHALL match in-host tab-drag behavior.

## Impact

- **SlintSystem:** `tickGlobalDockPointerPoll`, `wireNativeFloatingCallbacks`, drag-end routing.
- **DockManager:** `computeSlot`, optional `dragNeedsGlobalPoll()` / source-origin tracking.
- **ChildWindowRegistry / DockFloatingWindowHost:** coordinate helpers, hide/show float HWND during tab drag.
- **floating_panel_window.slint:** edge hover tint (optional), no behavioral change to resize logic.
- **Platform:** child-window cursor handling (Windows/SDL v1).
- **Depends on:** `dock-floating-os-window-drag-preview` (Phase B code paths); does not change viewport-in-main-window policy.
