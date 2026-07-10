## 1. Core types and configuration

- [x] 1.1 Add `DockEdge` (left/right/top/bottom), `DockAutoHideFlag`, `DockWidgetFeature` to `dock_types.h`
- [x] 1.2 Add `DockRestoreHint` and `DockAutoHideEntry` struct; extend `DockLayoutMetrics` with `auto_hide_strip_thickness`, overlay min/max defaults
- [x] 1.3 Add `DockAutoHideTabView`, `DockAutoHideOverlayView`, `DockAutoHideResizeHandleView` to `dock_layout_model.h`
- [x] 1.4 Extend `DockWidget` with `features()`, `isAutoHide()`, `autoHideEdge()` accessors
- [x] 1.5 Add `dock_auto_hide.h/.cpp` helpers (edge is horizontal, inner resize edge, max overlay size)

## 2. DockManager auto-hide logic

- [x] 2.1 Add edge lists, config flags, and restore hints to `DockManager`
- [x] 2.2 Implement `setWidgetAutoHide(widget_id, edge, enable)` — detach from container, push entry, store restore hint
- [x] 2.3 Implement `unpinAutoHide`, `toggleAutoHideExpanded`, `collapseAutoHide`, `collapseAllAutoHide`
- [x] 2.4 Update `buildLayoutModel()`: compute strip insets → solve main tree in inset rect → emit auto-hide tab/overlay views
- [x] 2.5 Implement overlay resize: `beginAutoHideResize`, `updateAutoHideResize`, `endAutoHideResize` with 64 px min
- [x] 2.6 Reject pin for `DockPanelKind::viewport`; default pinnable for hierarchy, inspector, content_browser, custom
- [x] 2.7 On unpin with invalid restore hint, `dockToRoot` to opposite edge slot

## 3. Slint presentation

- [x] 3.1 Add `DockAutoHideTab`, `DockAutoHideOverlay` structs and `DockAutoHideTabsLayer`, `DockAutoHideOverlaysLayer` to `docking_panel.slint`
- [x] 3.2 Wire `docking-auto-hide-tabs` / `docking-auto-hide-overlays` inside `docking_host` (dock-local coords; overlay chrome `visible` only when expanded)
- [x] 3.3 Dispatch overlay panel content by `panel_kind` (reuse Hierarchy, Inspector, ContentBrowser, Viewport placeholder rules)
- [x] 3.4 Add pin affordance on dock tab (icon or context) calling `setWidgetAutoHide` when `dock_area_has_pin_button` is set
- [x] 3.5 Add overlay title bar with optional minimize (collapse) and close buttons per flags

## 4. SlintSystem integration

- [x] 4.1 Extend `syncDockingWorkspace()` to push auto-hide models to `MainEditorWindow`
- [x] 4.2 Wire callbacks: tab pressed/hover/unpin, overlay resize, collapse, close
- [x] 4.3 Implement hover delay timer (`show_on_mouse_over`) and cancel on mouse down (ADS `handleAutoHideWidgetEvent` semantics)
- [x] 4.4 Implement click-outside collapse on SDL mouse up when `close_on_outside_click` is set
- [x] 4.5 `isDockLayoutDragActive()` wired for input routing; dock drag no longer blocks `shouldDeferHeavyFrameWork()` (Skia/UI must refresh during tab drag)
- [x] 4.6 Fix `isSplitterResizeInteractionActive()` → splitter drag OR auto-hide resize active
- [x] 4.7 Verify viewport/hierarchy/browser rect caches update after pin/unpin/expand/collapse

## 5. Bridge and CMake

- [x] 5.1 Extend `DockSlintBridge` (or `SlintSystem` inline helpers) to build auto-hide vector models
- [x] 5.2 Register new sources in `engine/src/runtime/CMakeLists.txt`; ensure `docking_panel.slint` is in `slint_target_sources`

## 6. Validation

- [x] 6.1 Build `engine_runtime` (Debug) — Slint + docking TUs compile; `engine_editor` link fails with LNK1168 if exe is already running
- [x] 6.2 Manual: pin Hierarchy left → viewport widens; tab visible; click expands overlay
- [x] 6.3 Manual: unpin → Hierarchy returns to docked column; layout matches pre-pin proportions
- [x] 6.4 Manual: resize overlay handle respects min 64 px and host max
- [x] 6.5 Manual: click outside collapses overlay (default config); viewport orbit still works when overlay collapsed
- [x] 6.6 Manual: drag dock tab still works; splitter defer does not regress viewport pacing during resize
- [x] 6.7 Manual: pin viewport attempt rejected; editor remains usable
