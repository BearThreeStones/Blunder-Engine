## 1. Spike and feature gates

- [x] 1.1 Spike: create borderless SDL child window parented to main HWND; confirm move/resize and event delivery
- [x] 1.2 Spike: second Slint `WindowAdapter` + SkiaRenderer on child HWND (non-viewport panel only)
- [x] 1.3 Add `DockFloatingFlag::native_os_window` and `DockFloatingFlag::drag_preview` to docking config; default preview on, native OS on for Windows only

## 2. Non-Opaque drag preview (Phase A)

- [x] 2.1 Extend `DockDragController` with threshold, `preview_active`, `preview_rect`, and cancel (Escape) support
- [x] 2.2 Add `DockDragPreviewView` to `DockLayoutModel`; populate from drag controller in `buildLayoutModel()`
- [x] 2.3 Create `DockFloatingDragPreviewLayer` in Slint (semi-transparent frame + title, ~60% opacity, no hit-test)
- [x] 2.4 Wire preview into `editor_window.slint` above tiles, below tabs; sync via `SlintSystem::syncDockingWorkspace`
- [x] 2.5 Update tab drag handlers: show preview after threshold; hide on release; guides still use pointer in dock-local coords
- [x] 2.6 Verify: drag Hierarchy tab → preview follows mouse; release on guide docks; release outside commits float; Esc cancels

## 3. Platform child windows (Phase B foundation)

- [x] 3.1 Extend `WindowSystem` (or add `ChildWindowRegistry`) with create/destroy/move/resize/raise for SDL child windows
- [x] 3.2 Implement screen ↔ main-client coordinate helpers (Win32 `ClientToScreen` / `ScreenToClient`)
- [x] 3.3 Create `dock_floating_window_host.h/.cpp` mapping `DockId` → `SDL_Window*` + optional Slint adapter handle

## 4. Native OS floating windows (Phase B)

- [x] 4.1 Create `floating_panel_window.slint` with title bar, close, resize grip, and panel-kind content slot
- [x] 4.2 On `makeFloatingFor` (non-viewport, flag on): create native window via host; skip in-host `docking-floatings` for that node
- [x] 4.3 Route Hierarchy / Inspector panel state to child Slint window; remove duplicate rendering from main `docking-tiles`
- [x] 4.4 Wire title drag and grip resize to `DockManager` floating interaction with coordinate conversion
- [x] 4.5 Wire close button to `closeWidget` / tear down float and destroy HWND
- [x] 4.6 Viewport float: keep in-host `docking-floatings` path when viewport is floated (no native window)
- [x] 4.7 OS float window: 8-edge resize strips (N/S/E/W + corners) via `DockResizeEdge`

## 5. Re-dock and integration (Phase C)

- [x] 5.1 Ensure tab drag from native float window hits main dock `hitTest` / guides correctly
- [x] 5.2 On successful re-dock, destroy native window when float node is removed
- [x] 5.3 Raise native window z-order on activation / title-bar click
- [x] 5.4 Add Content Browser to native float panel routing
- [x] 5.5 Fallback: when `native_os_window` off, retain existing `docking-floatings` behavior unchanged
- [x] 5.6 Global pointer poll (`tickGlobalDockPointerPoll`) for cross-HWND tab drag and float resize

## 6. Build and validation

- [x] 6.1 Build `engine_editor` Debug on Windows
- [x] 6.2 Manual: drag preview visible and semi-transparent during tab drag
- [ ] 6.3 Manual: Hierarchy/Inspector float to OS window, move outside main window bounds, resize, close
- [ ] 6.4 Manual: re-dock from OS float via guides; Viewport float stays in main window
- [ ] 6.5 Manual: feature flag off restores in-host floats only
