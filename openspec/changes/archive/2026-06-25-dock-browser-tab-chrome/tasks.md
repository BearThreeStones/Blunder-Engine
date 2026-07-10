## 1. DockChromeBar component

- [x] 1.1 Add `DockChromeBar` to `docking_panel.slint`: tab strip (reuse `DockTabsLayer` tab drag/close), chrome blank `title-drag` strip, top-right window ×
- [x] 1.2 Define shared `chrome-bar-height` (~32px) and browser-style tab visuals (active/inactive, top-left alignment)
- [x] 1.3 Expose callbacks: tab-* (unchanged), `chrome-move-*` (float window move), `window-close-pressed`

## 2. Native OS float window

- [x] 2.1 Replace title row + separate `DockTabsLayer` in `floating_panel_window.slint` with `DockChromeBar`
- [x] 2.2 Wire `chrome-move-*` to existing `title-move-*` callbacks in `dock_floating_window_host.cpp`
- [x] 2.3 Wire `window-close-pressed` to existing `close-pressed`; keep tab × on `widget-closed` path (option B)
- [x] 2.4 Remove redundant `title-text` row; derive window label from active tab only if needed for OS window title (optional)
- [x] 2.5 Build OS float tabs in `syncTabs` from `floatingContent` widgets (not `model.tabs`; native float skips layout model)

## 3. Main window DockingHost

- [x] 3.1 Position `DockChromeBar` on each container `tab_bar_rect` in `editor_window.slint` (replace global-only `DockTabsLayer` overlay pattern where needed)
- [x] 3.2 Update in-host `docking-floatings` frames to use `DockChromeBar` instead of title-only rectangle
- [x] 3.3 Ensure tab coordinates passed to C++ remain dock-local (same callback signatures)

## 4. Panel header deduplication

- [x] 4.1 Remove `panel-label` duplicate titles from `DockingHost` content tiles in `editor_window.slint`
- [x] 4.2 Remove duplicate static panel name headers from float content wrappers where chrome tab shows the name
- [x] 4.3 Verify Hierarchy / Inspector / Content Browser functional UI (path bar, fields) unchanged

## 5. C++ layout metrics and hit-testing

- [x] 5.1 Add `chrome_bar_height` to `DockLayoutMetrics`; use for `tab_bar_rect` and float chrome inset
- [x] 5.2 Update `hitFloatResizeEdge` to single-row `k_chrome_top` (remove `title + tab` sum)
- [x] 5.3 Align `appendFloatingFrame` / `appendContainerTile` with unified chrome height
- [x] 5.4 Document or share constant between Slint `chrome-bar-height` and C++ `constexpr`

## 6. Manual validation

- [x] 6.1 OS float: single chrome row; drag tab → re-dock with guides (update steps: drag top chrome tab, not second row)
- [x] 6.2 OS float: drag chrome blank strip moves window; drag tab does not move window
- [x] 6.3 OS float: window × closes float; tab × closes only that widget (multi-tab case)
- [x] 6.4 Main window: docked panels show browser chrome; no duplicate "Hierarchy" in content
- [x] 6.5 Resize cursors on OS float content edges still work below chrome row
- [x] 6.6 Re-run `dock-float-redock-guide-stability` scenarios 6.3–6.5 after chrome lands
